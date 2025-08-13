// ==========================================
// vfx/vehicleglass/VehicleGlassSmashTest.cpp
// (c) 2012-2013 Rockstar Games
// ==========================================

#include "vfx/vehicleglass/VehicleGlass.h"

#if __DEV
#include "fwmaths/vectorutil.h"
#include "vfx/vehicleglass/vehicleglassconstructor.h"
#include "vfx/vehicleglass/vehicleglasswindow.h"
#include "vfx/vehicleglass/VehicleGlassManager.h"
#include "vfx/VfxHelper.h" // for crack test
#endif // __DEV

#include "file/default_paths.h"
#include "system/nelem.h"
#include "vehicles/vehicle.h"

#include "fwutil/xmacro.h"

#include "renderer/HierarchyIds.h"

// moved this outside of the smash test so that it will force people to update when they add or remove hierarchy IDs ..
const char** GetVehicleHierarchyIdNames()
{
#define FOREACH_DEF_CheckVehicleHierarchyId(DEF) \
	DEF(VEH_CHASSIS                      ) \
	DEF(VEH_CHASSIS_LOWLOD               ) \
	DEF(VEH_CHASSIS_DUMMY                ) \
	DEF(VEH_DOOR_DSIDE_F                 ) \
	DEF(VEH_DOOR_DSIDE_R                 ) \
	DEF(VEH_DOOR_PSIDE_F                 ) \
	DEF(VEH_DOOR_PSIDE_R                 ) \
	DEF(VEH_HANDLE_DSIDE_F               ) \
	DEF(VEH_HANDLE_DSIDE_R               ) \
	DEF(VEH_HANDLE_PSIDE_F               ) \
	DEF(VEH_HANDLE_PSIDE_R               ) \
	DEF(VEH_WHEEL_LF                     ) \
	DEF(VEH_WHEEL_RF                     ) \
	DEF(VEH_WHEEL_LR                     ) \
	DEF(VEH_WHEEL_RR                     ) \
	DEF(VEH_WHEEL_LM1                    ) \
	DEF(VEH_WHEEL_RM1                    ) \
	DEF(VEH_WHEEL_LM2                    ) \
	DEF(VEH_WHEEL_RM2                    ) \
	DEF(VEH_WHEEL_LM3                    ) \
	DEF(VEH_WHEEL_RM3                    ) \
	DEF(VEH_SUSPENSION_LF                ) \
	DEF(VEH_SUSPENSION_RF                ) \
	DEF(VEH_SUSPENSION_LM                ) \
	DEF(VEH_SUSPENSION_RM                ) \
	DEF(VEH_SUSPENSION_LR                ) \
	DEF(VEH_SUSPENSION_RR                ) \
	DEF(VEH_TRANSMISSION_F               ) \
	DEF(VEH_TRANSMISSION_M               ) \
	DEF(VEH_TRANSMISSION_R               ) \
	DEF(VEH_WHEELHUB_LF                  ) \
	DEF(VEH_WHEELHUB_RF                  ) \
	DEF(VEH_WHEELHUB_LR                  ) \
	DEF(VEH_WHEELHUB_RR                  ) \
	DEF(VEH_WHEELHUB_LM1                 ) \
	DEF(VEH_WHEELHUB_RM1                 ) \
	DEF(VEH_WHEELHUB_LM2                 ) \
	DEF(VEH_WHEELHUB_RM2                 ) \
	DEF(VEH_WHEELHUB_LM3                 ) \
	DEF(VEH_WHEELHUB_RM3                 ) \
	DEF(VEH_WINDSCREEN                   ) \
	DEF(VEH_WINDSCREEN_R                 ) \
	DEF(VEH_WINDOW_LF                    ) \
	DEF(VEH_WINDOW_RF                    ) \
	DEF(VEH_WINDOW_LR                    ) \
	DEF(VEH_WINDOW_RR                    ) \
	DEF(VEH_WINDOW_LM                    ) \
	DEF(VEH_WINDOW_RM                    ) \
	DEF(VEH_BODYSHELL                    ) \
	DEF(VEH_BUMPER_F                     ) \
	DEF(VEH_BUMPER_R                     ) \
	DEF(VEH_WING_RF                      ) \
	DEF(VEH_WING_LF                      ) \
	DEF(VEH_BONNET                       ) \
	DEF(VEH_BOOT                         ) \
	DEF(VEH_BOOT_2                       ) \
	DEF(VEH_EXHAUST                      ) \
	DEF(VEH_EXHAUST_2                    ) \
	DEF(VEH_EXHAUST_3                    ) \
	DEF(VEH_EXHAUST_4                    ) \
	DEF(VEH_EXHAUST_5                    ) \
	DEF(VEH_EXHAUST_6                    ) \
	DEF(VEH_EXHAUST_7                    ) \
	DEF(VEH_EXHAUST_8                    ) \
	DEF(VEH_EXHAUST_9                    ) \
	DEF(VEH_EXHAUST_10                   ) \
	DEF(VEH_EXHAUST_11                   ) \
	DEF(VEH_EXHAUST_12                   ) \
	DEF(VEH_EXHAUST_13                   ) \
	DEF(VEH_EXHAUST_14                   ) \
	DEF(VEH_EXHAUST_15                   ) \
	DEF(VEH_EXHAUST_16                   ) \
	DEF(VEH_EXHAUST_17                      ) \
	DEF(VEH_EXHAUST_18                    ) \
	DEF(VEH_EXHAUST_19                    ) \
	DEF(VEH_EXHAUST_20                    ) \
	DEF(VEH_EXHAUST_21                    ) \
	DEF(VEH_EXHAUST_22                    ) \
	DEF(VEH_EXHAUST_23                    ) \
	DEF(VEH_EXHAUST_24                    ) \
	DEF(VEH_EXHAUST_25                    ) \
	DEF(VEH_EXHAUST_26                   ) \
	DEF(VEH_EXHAUST_27                   ) \
	DEF(VEH_EXHAUST_28                   ) \
	DEF(VEH_EXHAUST_29                   ) \
	DEF(VEH_EXHAUST_30                   ) \
	DEF(VEH_EXHAUST_31                   ) \
	DEF(VEH_EXHAUST_32                   ) \
	DEF(VEH_ENGINE                       ) \
	DEF(VEH_OVERHEAT                     ) \
	DEF(VEH_OVERHEAT_2                   ) \
	DEF(VEH_PETROLCAP                    ) \
	DEF(VEH_PETROLTANK                   ) \
	DEF(VEH_PETROLTANK_L                 ) \
	DEF(VEH_PETROLTANK_R                 ) \
	DEF(VEH_STEERING_WHEEL               ) \
	DEF(VEH_CAR_STEERING_WHEEL           ) \
	DEF(VEH_HBGRIP_L                     ) \
	DEF(VEH_HBGRIP_R                     ) \
	DEF(VEH_ROCKET_BOOST                 ) \
	DEF(VEH_ROCKET_BOOST_2               ) \
	DEF(VEH_ROCKET_BOOST_3               ) \
	DEF(VEH_ROCKET_BOOST_4               ) \
	DEF(VEH_ROCKET_BOOST_5               ) \
	DEF(VEH_ROCKET_BOOST_6               ) \
	DEF(VEH_ROCKET_BOOST_7               ) \
	DEF(VEH_ROCKET_BOOST_8               ) \
	DEF(VEH_HEADLIGHT_L                  ) \
	DEF(VEH_HEADLIGHT_R                  ) \
	DEF(VEH_TAILLIGHT_L                  ) \
	DEF(VEH_TAILLIGHT_R                  ) \
	DEF(VEH_INDICATOR_LF                 ) \
	DEF(VEH_INDICATOR_RF                 ) \
	DEF(VEH_INDICATOR_LR                 ) \
	DEF(VEH_INDICATOR_RR                 ) \
	DEF(VEH_BRAKELIGHT_L                 ) \
	DEF(VEH_BRAKELIGHT_R                 ) \
	DEF(VEH_BRAKELIGHT_M                 ) \
	DEF(VEH_REVERSINGLIGHT_L             ) \
	DEF(VEH_REVERSINGLIGHT_R             ) \
	DEF(VEH_NEON_L                       ) \
	DEF(VEH_NEON_R                       ) \
	DEF(VEH_NEON_F                       ) \
	DEF(VEH_NEON_B                       ) \
	DEF(VEH_EXTRALIGHT_1                 ) \
	DEF(VEH_EXTRALIGHT_2                 ) \
	DEF(VEH_EXTRALIGHT_3                 ) \
	DEF(VEH_EXTRALIGHT_4                 ) \
	DEF(VEH_EMISSIVES					 ) \
	DEF(VEH_NUMBERPLATE                  ) \
	DEF(VEH_INTERIORLIGHT                ) \
	GTA_ONLY(DEF(VEH_PLATELIGHT         )) \
	GTA_ONLY(DEF(VEH_DASHLIGHT          )) \
	GTA_ONLY(DEF(VEH_DOORLIGHT_LF       )) \
	GTA_ONLY(DEF(VEH_DOORLIGHT_RF       )) \
	GTA_ONLY(DEF(VEH_DOORLIGHT_LR       )) \
	GTA_ONLY(DEF(VEH_DOORLIGHT_RR       )) \
	DEF(VEH_SIREN_1                      ) \
	DEF(VEH_SIREN_2                      ) \
	DEF(VEH_SIREN_3                      ) \
	DEF(VEH_SIREN_4                      ) \
	DEF(VEH_SIREN_5                      ) \
	DEF(VEH_SIREN_6                      ) \
	DEF(VEH_SIREN_7                      ) \
	DEF(VEH_SIREN_8                      ) \
	DEF(VEH_SIREN_9                      ) \
	DEF(VEH_SIREN_10                     ) \
	DEF(VEH_SIREN_11                     ) \
	DEF(VEH_SIREN_12                     ) \
	DEF(VEH_SIREN_13                     ) \
	DEF(VEH_SIREN_14                     ) \
	DEF(VEH_SIREN_15                     ) \
	DEF(VEH_SIREN_16                     ) \
	DEF(VEH_SIREN_17                     ) \
	DEF(VEH_SIREN_18                     ) \
	DEF(VEH_SIREN_19                     ) \
	DEF(VEH_SIREN_20                     ) \
	DEF(VEH_SIREN_GLASS_1                ) \
	DEF(VEH_SIREN_GLASS_2                ) \
	DEF(VEH_SIREN_GLASS_3                ) \
	DEF(VEH_SIREN_GLASS_4                ) \
	DEF(VEH_SIREN_GLASS_5                ) \
	DEF(VEH_SIREN_GLASS_6                ) \
	DEF(VEH_SIREN_GLASS_7                ) \
	DEF(VEH_SIREN_GLASS_8                ) \
	DEF(VEH_SIREN_GLASS_9                ) \
	DEF(VEH_SIREN_GLASS_10               ) \
	DEF(VEH_SIREN_GLASS_11               ) \
	DEF(VEH_SIREN_GLASS_12               ) \
	DEF(VEH_SIREN_GLASS_13               ) \
	DEF(VEH_SIREN_GLASS_14               ) \
	DEF(VEH_SIREN_GLASS_15               ) \
	DEF(VEH_SIREN_GLASS_16               ) \
	DEF(VEH_SIREN_GLASS_17               ) \
	DEF(VEH_SIREN_GLASS_18               ) \
	DEF(VEH_SIREN_GLASS_19               ) \
	DEF(VEH_SIREN_GLASS_20               ) \
	DEF(VEH_WEAPON_1A                    ) \
	DEF(VEH_WEAPON_1B                    ) \
	DEF(VEH_WEAPON_1C                    ) \
	DEF(VEH_WEAPON_1D                    ) \
	DEF(VEH_WEAPON_1A_ROT                ) \
	DEF(VEH_WEAPON_1B_ROT                ) \
	DEF(VEH_WEAPON_1C_ROT                ) \
	DEF(VEH_WEAPON_1D_ROT                ) \
	DEF(VEH_WEAPON_2A                    ) \
	DEF(VEH_WEAPON_2B                    ) \
	DEF(VEH_WEAPON_2C                    ) \
	DEF(VEH_WEAPON_2D                    ) \
	DEF(VEH_WEAPON_2E                    ) \
	DEF(VEH_WEAPON_2F                    ) \
	DEF(VEH_WEAPON_2G                    ) \
	DEF(VEH_WEAPON_2H                    ) \
	DEF(VEH_WEAPON_2A_ROT                ) \
	DEF(VEH_WEAPON_2B_ROT                ) \
	DEF(VEH_WEAPON_2C_ROT                ) \
	DEF(VEH_WEAPON_2D_ROT				 ) \
	DEF(VEH_WEAPON_3A                    ) \
	DEF(VEH_WEAPON_3B                    ) \
	DEF(VEH_WEAPON_3C                    ) \
	DEF(VEH_WEAPON_3D                    ) \
	DEF(VEH_WEAPON_3A_ROT                ) \
	DEF(VEH_WEAPON_3B_ROT                ) \
	DEF(VEH_WEAPON_3C_ROT                ) \
	DEF(VEH_WEAPON_3D_ROT				 ) \
	DEF(VEH_WEAPON_4A                    ) \
	DEF(VEH_WEAPON_4B                    ) \
	DEF(VEH_WEAPON_4C                    ) \
	DEF(VEH_WEAPON_4D                    ) \
	DEF(VEH_WEAPON_4A_ROT                ) \
	DEF(VEH_WEAPON_4B_ROT                ) \
	DEF(VEH_WEAPON_4C_ROT                ) \
	DEF(VEH_WEAPON_4D_ROT				 ) \
	DEF(VEH_TURRET_1_BASE                ) \
	DEF(VEH_TURRET_1_BARREL              ) \
	DEF(VEH_TURRET_2_BASE                ) \
	DEF(VEH_TURRET_2_BARREL              ) \
	DEF(VEH_TURRET_3_BASE                ) \
	DEF(VEH_TURRET_3_BARREL              ) \
	DEF(VEH_TURRET_4_BASE                ) \
	DEF(VEH_TURRET_4_BARREL              ) \
	DEF(VEH_WEAPON_5A			 ) \
	DEF(VEH_WEAPON_5B			 ) \
	DEF(VEH_WEAPON_5C			 ) \
	DEF(VEH_WEAPON_5D			 ) \
	DEF(VEH_WEAPON_5E			 ) \
	DEF(VEH_WEAPON_5F			 ) \
	DEF(VEH_WEAPON_5G			 ) \
	DEF(VEH_WEAPON_5H			 ) \
	DEF(VEH_WEAPON_5I			 ) \
	DEF(VEH_WEAPON_5J			 ) \
	DEF(VEH_WEAPON_5K			 ) \
	DEF(VEH_WEAPON_5L			 ) \
	DEF(VEH_WEAPON_5M			 ) \
	DEF(VEH_WEAPON_5N			 ) \
	DEF(VEH_WEAPON_5O			 ) \
	DEF(VEH_WEAPON_5P			 ) \
	DEF(VEH_WEAPON_5Q			 ) \
	DEF(VEH_WEAPON_5R			 ) \
	DEF(VEH_WEAPON_5S			 ) \
	DEF(VEH_WEAPON_5T			 ) \
	DEF(VEH_WEAPON_5U			 ) \
	DEF(VEH_WEAPON_5V			 ) \
	DEF(VEH_WEAPON_5W			 ) \
	DEF(VEH_WEAPON_5X			 ) \
	DEF(VEH_WEAPON_5Y			 ) \
	DEF(VEH_WEAPON_5Z			 ) \
	DEF(VEH_WEAPON_5AA			 ) \
	DEF(VEH_WEAPON_5AB			 ) \
	DEF(VEH_WEAPON_5AC			 ) \
	DEF(VEH_WEAPON_5AD			 ) \
	DEF(VEH_WEAPON_5AE			 ) \
	DEF(VEH_WEAPON_5AF			 ) \
	DEF(VEH_WEAPON_5AG			 ) \
	DEF(VEH_WEAPON_5AH			 ) \
	DEF(VEH_WEAPON_5AI			 ) \
	DEF(VEH_WEAPON_5AJ			 ) \
	DEF(VEH_WEAPON_5AK			 ) \
	DEF(VEH_WEAPON_5AL			 ) \
	DEF(VEH_WEAPON_5A_ROT			 ) \
	DEF(VEH_WEAPON_5B_ROT			 ) \
	DEF(VEH_WEAPON_5C_ROT			 ) \
	DEF(VEH_WEAPON_5D_ROT			 ) \
	DEF(VEH_WEAPON_5E_ROT			 ) \
	DEF(VEH_WEAPON_5F_ROT			 ) \
	DEF(VEH_WEAPON_5G_ROT			 ) \
	DEF(VEH_WEAPON_5H_ROT			 ) \
	DEF(VEH_WEAPON_5I_ROT			 ) \
	DEF(VEH_WEAPON_5J_ROT			 ) \
	DEF(VEH_WEAPON_5K_ROT			 ) \
	DEF(VEH_WEAPON_5L_ROT			 ) \
	DEF(VEH_WEAPON_5M_ROT			 ) \
	DEF(VEH_WEAPON_5N_ROT			 ) \
	DEF(VEH_WEAPON_5O_ROT			 ) \
	DEF(VEH_WEAPON_5P_ROT			 ) \
	DEF(VEH_WEAPON_5Q_ROT			 ) \
	DEF(VEH_WEAPON_5R_ROT			 ) \
	DEF(VEH_WEAPON_5S_ROT			 ) \
	DEF(VEH_WEAPON_5T_ROT			 ) \
	DEF(VEH_WEAPON_5U_ROT			 ) \
	DEF(VEH_WEAPON_5V_ROT			 ) \
	DEF(VEH_WEAPON_5W_ROT			 ) \
	DEF(VEH_WEAPON_5X_ROT			 ) \
	DEF(VEH_WEAPON_5Y_ROT			 ) \
	DEF(VEH_WEAPON_5Z_ROT			 ) \
	DEF(VEH_WEAPON_5AA_ROT			 ) \
	DEF(VEH_WEAPON_5AB_ROT			 ) \
	DEF(VEH_WEAPON_5AC_ROT			 ) \
	DEF(VEH_WEAPON_5AD_ROT			 ) \
	DEF(VEH_WEAPON_5AE_ROT			 ) \
	DEF(VEH_WEAPON_5AF_ROT			 ) \
	DEF(VEH_WEAPON_5AG_ROT			 ) \
	DEF(VEH_WEAPON_5AH_ROT			 ) \
	DEF(VEH_WEAPON_5AI_ROT			 ) \
	DEF(VEH_WEAPON_5AJ_ROT			 ) \
	DEF(VEH_WEAPON_5AK_ROT			 ) \
	DEF(VEH_WEAPON_5AL_ROT			 ) \
	DEF(VEH_WEAPON_6A			 ) \
	DEF(VEH_WEAPON_6B			 ) \
	DEF(VEH_WEAPON_6C			 ) \
	DEF(VEH_WEAPON_6D			 ) \
	DEF(VEH_WEAPON_6A_ROT			 ) \
	DEF(VEH_WEAPON_6B_ROT			 ) \
	DEF(VEH_WEAPON_6C_ROT			 ) \
	DEF(VEH_WEAPON_6D_ROT			 ) \
	DEF(VEH_TURRET_AMMO_BELT			 ) \
	DEF(VEH_SEARCHLIGHT_BASE             ) \
	DEF(VEH_SEARCHLIGHT_BARREL           ) \
	DEF(VEH_ATTACH                       ) \
	DEF(VEH_ROOF                         ) \
	DEF(VEH_ROOF2                        ) \
	DEF(VEH_SOFTTOP_1                    ) \
	DEF(VEH_SOFTTOP_2                    ) \
	DEF(VEH_SOFTTOP_3                    ) \
	DEF(VEH_SOFTTOP_4                    ) \
	DEF(VEH_SOFTTOP_5                    ) \
	DEF(VEH_SOFTTOP_6                    ) \
	DEF(VEH_SOFTTOP_7                    ) \
	DEF(VEH_SOFTTOP_8                    ) \
	DEF(VEH_SOFTTOP_9                    ) \
	DEF(VEH_SOFTTOP_10                   ) \
	DEF(VEH_SOFTTOP_11                   ) \
	DEF(VEH_SOFTTOP_12                   ) \
	DEF(VEH_SOFTTOP_13                   ) \
	DEF(VEH_SOFT1                        ) \
	DEF(VEH_PONTOON_L					 ) \
	DEF(VEH_PONTOON_R				     ) \
	DEF(VEH_FORKS                        ) \
	DEF(VEH_MAST                         ) \
	DEF(VEH_CARRIAGE                     ) \
	DEF(VEH_FORK_LEFT					 ) \
	DEF(VEH_FORK_RIGHT					 ) \
	DEF(VEH_FORKS_ATTACH                 ) \
	DEF(VEH_HANDLER_FRAME_1              ) \
	DEF(VEH_HANDLER_FRAME_2              ) \
	DEF(VEH_HANDLER_FRAME_3				 ) \
	DEF(VEH_HANDLER_FRAME_PICKUP_1		 ) \
	DEF(VEH_HANDLER_FRAME_PICKUP_2		 ) \
	DEF(VEH_HANDLER_FRAME_PICKUP_3		 ) \
	DEF(VEH_HANDLER_FRAME_PICKUP_4		 ) \
	DEF(VEH_DODO_NO_PED_COL_STEP_L	   	 ) \
	DEF(VEH_DODO_NO_PED_COL_STEP_R		 ) \
	DEF(VEH_DODO_NO_PED_COL_STRUT_1_L	 ) \
	DEF(VEH_DODO_NO_PED_COL_STRUT_2_L	 ) \
	DEF(VEH_DODO_NO_PED_COL_STRUT_1_R	 ) \
	DEF(VEH_DODO_NO_PED_COL_STRUT_2_R	 ) \
	DEF(VEH_FREIGHTCONT2_CONTAINER		 ) \
	DEF(VEH_FREIGHTGRAIN_SLIDING_DOOR	 ) \
	DEF(VEH_FREIGHTCONT2_BOGEY			 ) \
	DEF(VEH_DOOR_HATCH_R                 ) \
	DEF(VEH_DOOR_HATCH_L                 ) \
	DEF(VEH_TOW_ARM                      ) \
	DEF(VEH_TOW_MOUNT_A                  ) \
	DEF(VEH_TOW_MOUNT_B                  ) \
	DEF(VEH_TIPPER                       ) \
	DEF(VEH_PICKUP_REEL                  ) \
	DEF(VEH_AUGER                        ) \
	DEF(VEH_SLIPSTREAM_L                 ) \
	DEF(VEH_SLIPSTREAM_R                 ) \
	DEF(VEH_ARM1                         ) \
	DEF(VEH_ARM2                         ) \
	DEF(VEH_ARM3                         ) \
	DEF(VEH_ARM4                         ) \
	DEF(VEH_DIGGER_ARM                   ) \
	DEF(VEH_ARTICULATED_DIGGER_ARM_BASE  ) \
	DEF(VEH_ARTICULATED_DIGGER_ARM_BOOM  ) \
	DEF(VEH_ARTICULATED_DIGGER_ARM_STICK ) \
	DEF(VEH_ARTICULATED_DIGGER_ARM_BUCKET) \
	DEF(VEH_CUTTER_ARM_1                 ) \
	DEF(VEH_CUTTER_ARM_2                 ) \
	DEF(VEH_CUTTER_BOOM_DRIVER           ) \
	DEF(VEH_CUTTER_CUTTER_DRIVER         ) \
	DEF(VEH_DIGGER_SHOVEL_2              ) \
	DEF(VEH_DIGGER_SHOVEL_3              ) \
	DEF(VEH_VEHICLE_BLOCKER              ) \
	GTA_ONLY(DEF(VEH_DIALS              )) \
	GTA_ONLY(DEF(VEH_LIGHTCOVER         )) \
	GTA_ONLY(DEF(VEH_BOBBLE_HEAD        )) \
	GTA_ONLY(DEF(VEH_BOBBLE_BASE        )) \
	GTA_ONLY(DEF(VEH_BOBBLE_HAND        )) \
	GTA_ONLY(DEF(VEH_BOBBLE_ENGINE      )) \
	GTA_ONLY(DEF(VEH_SPOILER			)) \
	GTA_ONLY(DEF(VEH_STRUTS				)) \
	GTA_ONLY(DEF(VEH_FLAP_L				)) \
	GTA_ONLY(DEF(VEH_FLAP_R				)) \
	DEF(VEH_MISC_A                       ) \
	DEF(VEH_MISC_B                       ) \
	DEF(VEH_MISC_C                       ) \
	DEF(VEH_MISC_D                       ) \
	DEF(VEH_MISC_E                       ) \
	DEF(VEH_MISC_F                       ) \
	DEF(VEH_MISC_G                       ) \
	DEF(VEH_MISC_H                       ) \
	DEF(VEH_MISC_I                       ) \
	DEF(VEH_MISC_J                       ) \
	DEF(VEH_MISC_K                       ) \
	DEF(VEH_MISC_L                       ) \
	DEF(VEH_MISC_M                       ) \
	DEF(VEH_MISC_N                       ) \
	DEF(VEH_MISC_O                       ) \
	DEF(VEH_MISC_P                       ) \
	DEF(VEH_MISC_Q                       ) \
	DEF(VEH_MISC_R                       ) \
	DEF(VEH_MISC_S                       ) \
	DEF(VEH_MISC_T                       ) \
	DEF(VEH_MISC_U                       ) \
	DEF(VEH_MISC_V                       ) \
	DEF(VEH_MISC_W                       ) \
	DEF(VEH_MISC_X                       ) \
	DEF(VEH_MISC_Y                       ) \
	DEF(VEH_MISC_Z                       ) \
	DEF(VEH_MISC_1                       ) \
	DEF(VEH_MISC_2                       ) \
	DEF(VEH_MISC_3                       ) \
	DEF(VEH_MISC_4                       ) \
	DEF(VEH_MISC_5                       ) \
	DEF(VEH_MISC_6                       ) \
	DEF(VEH_MISC_7                       ) \
	DEF(VEH_MISC_8                       ) \
	DEF(VEH_MISC_9                       ) \
	DEF(VEH_EXTRA_1                      ) \
	DEF(VEH_EXTRA_2                      ) \
	DEF(VEH_EXTRA_3                      ) \
	DEF(VEH_EXTRA_4                      ) \
	DEF(VEH_EXTRA_5                      ) \
	DEF(VEH_EXTRA_6                      ) \
	DEF(VEH_EXTRA_7                      ) \
	DEF(VEH_EXTRA_8                      ) \
	DEF(VEH_EXTRA_9                      ) \
	DEF(VEH_EXTRA_10                     ) \
	DEF(VEH_EXTRA_11                     ) \
	DEF(VEH_EXTRA_12                     ) \
	DEF(VEH_EXTRA_13                     ) \
	DEF(VEH_EXTRA_14                     ) \
	DEF(VEH_EXTRA_15                     ) \
	DEF(VEH_EXTRA_16                     ) \
	DEF(VEH_BREAKABLE_EXTRA_1			 ) \
	DEF(VEH_BREAKABLE_EXTRA_2			 ) \
	DEF(VEH_BREAKABLE_EXTRA_3			 ) \
	DEF(VEH_BREAKABLE_EXTRA_4			 ) \
	DEF(VEH_BREAKABLE_EXTRA_5			 ) \
	DEF(VEH_BREAKABLE_EXTRA_6			 ) \
	DEF(VEH_BREAKABLE_EXTRA_7			 ) \
	DEF(VEH_BREAKABLE_EXTRA_8			 ) \
	DEF(VEH_BREAKABLE_EXTRA_9			 ) \
	DEF(VEH_BREAKABLE_EXTRA_10			 ) \
	DEF(VEH_MOD_COLLISION_1				 ) \
	DEF(VEH_MOD_COLLISION_2				 ) \
	DEF(VEH_MOD_COLLISION_3				 ) \
	DEF(VEH_MOD_COLLISION_4				 ) \
	DEF(VEH_MOD_COLLISION_5				 ) \
	DEF(VEH_MOD_COLLISION_6				 ) \
	DEF(VEH_MOD_COLLISION_7				 ) \
	DEF(VEH_MOD_COLLISION_8				 ) \
	DEF(VEH_MOD_COLLISION_9				 ) \
	DEF(VEH_MOD_COLLISION_10			 ) \
	DEF(VEH_MOD_COLLISION_11			 ) \
	DEF(VEH_MOD_COLLISION_12			 ) \
	DEF(VEH_MOD_COLLISION_13			 ) \
	DEF(VEH_MOD_COLLISION_14			 ) \
	DEF(VEH_MOD_COLLISION_15			 ) \
	DEF(VEH_MOD_COLLISION_16			 ) \
	DEF(VEH_SPRING_RF                    ) \
	DEF(VEH_SPRING_LF                    ) \
	DEF(VEH_SPRING_RR                    ) \
	DEF(VEH_SPRING_LR                    ) \
	GTA_ONLY(DEF(VEH_PARACHUTE_OPEN		)) \
	GTA_ONLY(DEF(VEH_PARACHUTE_DEPLOY	)) \
	GTA_ONLY(DEF(VEH_RAMMING_SCOOP	)) \
	GTA_ONLY(DEF(VEH_TURRET_1_MOD	)) \
	GTA_ONLY(DEF(VEH_TURRET_2_MOD	)) \
	GTA_ONLY(DEF(VEH_TURRET_3_MOD	)) \
	GTA_ONLY(DEF(VEH_TURRET_4_MOD	)) \
	GTA_ONLY(DEF(AMPHIBIOUS_AUTOMOBILE_STATIC_PROP	)) \
	GTA_ONLY(DEF(AMPHIBIOUS_AUTOMOBILE_STATIC_PROP2	)) \
	GTA_ONLY(DEF(AMPHIBIOUS_AUTOMOBILE_MOVING_PROP	)) \
	GTA_ONLY(DEF(AMPHIBIOUS_AUTOMOBILE_MOVING_PROP2	)) \
	GTA_ONLY(DEF(AMPHIBIOUS_AUTOMOBILE_RUDDER	)) \
	GTA_ONLY(DEF(AMPHIBIOUS_AUTOMOBILE_RUDDER2	)) \
	GTA_ONLY(DEF(VEH_MISSILEFLAP_2A	)) \
	GTA_ONLY(DEF(VEH_MISSILEFLAP_2B	)) \
	GTA_ONLY(DEF(VEH_MISSILEFLAP_2C	)) \
	GTA_ONLY(DEF(VEH_MISSILEFLAP_2D	)) \
	GTA_ONLY(DEF(VEH_MISSILEFLAP_2E	)) \
	GTA_ONLY(DEF(VEH_MISSILEFLAP_2F	)) \
	GTA_ONLY(DEF(VEH_EXTENDABLE_SIDE_L	)) \
	GTA_ONLY(DEF(VEH_EXTENDABLE_SIDE_R	)) \
	GTA_ONLY(DEF(VEH_EXTENDABLE_A	)) \
	GTA_ONLY(DEF(VEH_EXTENDABLE_B	)) \
	GTA_ONLY(DEF(VEH_EXTENDABLE_C	)) \
	GTA_ONLY(DEF(VEH_EXTENDABLE_D	)) \
	GTA_ONLY(DEF(VEH_ROTATOR_L	)) \
	GTA_ONLY(DEF(VEH_ROTATOR_R	)) \
	GTA_ONLY(DEF(VEH_MOD_A	)) \
	GTA_ONLY(DEF(VEH_MOD_B	)) \
	GTA_ONLY(DEF(VEH_MOD_C	)) \
	GTA_ONLY(DEF(VEH_MOD_D	)) \
	GTA_ONLY(DEF(VEH_MOD_E	)) \
	GTA_ONLY(DEF(VEH_MOD_F	)) \
	GTA_ONLY(DEF(VEH_MOD_G	)) \
	GTA_ONLY(DEF(VEH_MOD_H	)) \
	GTA_ONLY(DEF(VEH_MOD_I	)) \
	GTA_ONLY(DEF(VEH_MOD_J	)) \
	GTA_ONLY(DEF(VEH_MOD_K	)) \
	GTA_ONLY(DEF(VEH_MOD_L	)) \
	GTA_ONLY(DEF(VEH_MOD_M	)) \
	GTA_ONLY(DEF(VEH_MOD_N	)) \
	GTA_ONLY(DEF(VEH_MOD_O	)) \
	GTA_ONLY(DEF(VEH_MOD_P	)) \
	GTA_ONLY(DEF(VEH_MOD_Q	)) \
	GTA_ONLY(DEF(VEH_MOD_R	)) \
	GTA_ONLY(DEF(VEH_MOD_S	)) \
	GTA_ONLY(DEF(VEH_MOD_T	)) \
	GTA_ONLY(DEF(VEH_MOD_U	)) \
	GTA_ONLY(DEF(VEH_MOD_V	)) \
	GTA_ONLY(DEF(VEH_MOD_W	)) \
	GTA_ONLY(DEF(VEH_MOD_X	)) \
	GTA_ONLY(DEF(VEH_MOD_Y	)) \
	GTA_ONLY(DEF(VEH_MOD_Z	)) \
	GTA_ONLY(DEF(VEH_MOD_AA	)) \
	GTA_ONLY(DEF(VEH_MOD_AB	)) \
	GTA_ONLY(DEF(VEH_MOD_AC	)) \
	GTA_ONLY(DEF(VEH_MOD_AD	)) \
	GTA_ONLY(DEF(VEH_MOD_AE	)) \
	GTA_ONLY(DEF(VEH_MOD_AF	)) \
	GTA_ONLY(DEF(VEH_MOD_AG	)) \
	GTA_ONLY(DEF(VEH_MOD_AH	)) \
	GTA_ONLY(DEF(VEH_MOD_AI	)) \
	GTA_ONLY(DEF(VEH_MOD_AJ	)) \
	GTA_ONLY(DEF(VEH_MOD_AK	)) \
	GTA_ONLY(DEF(VEH_TURRET_A1_BASE	)) \
	GTA_ONLY(DEF(VEH_TURRET_A1_BARREL	)) \
	GTA_ONLY(DEF(VEH_TURRET_A2_BASE	)) \
	GTA_ONLY(DEF(VEH_TURRET_A2_BARREL	)) \
	GTA_ONLY(DEF(VEH_TURRET_A3_BASE	)) \
	GTA_ONLY(DEF(VEH_TURRET_A3_BARREL	)) \
	GTA_ONLY(DEF(VEH_TURRET_A4_BASE	)) \
	GTA_ONLY(DEF(VEH_TURRET_A4_BARREL	)) \
	GTA_ONLY(DEF(VEH_TURRET_A_AMMO_BELT	)) \
	GTA_ONLY(DEF(VEH_TURRET_A1_MOD	)) \
	GTA_ONLY(DEF(VEH_TURRET_A2_MOD	)) \
	GTA_ONLY(DEF(VEH_TURRET_A3_MOD	)) \
	GTA_ONLY(DEF(VEH_TURRET_A4_MOD	)) \
	GTA_ONLY(DEF(VEH_TURRET_B1_MOD	)) \
	GTA_ONLY(DEF(VEH_TURRET_B2_MOD	)) \
	GTA_ONLY(DEF(VEH_TURRET_B3_MOD	)) \
	GTA_ONLY(DEF(VEH_TURRET_B4_MOD	)) \
	GTA_ONLY(DEF(VEH_TURRET_A1_BASE	)) \
	GTA_ONLY(DEF(VEH_TURRET_A1_BARREL	)) \
	GTA_ONLY(DEF(VEH_TURRET_A2_BASE	)) \
	GTA_ONLY(DEF(VEH_TURRET_A2_BARREL	)) \
	GTA_ONLY(DEF(VEH_TURRET_A3_BASE	)) \
	GTA_ONLY(DEF(VEH_TURRET_A3_BARREL	)) \
	GTA_ONLY(DEF(VEH_TURRET_A4_BASE	)) \
	GTA_ONLY(DEF(VEH_TURRET_A4_BARREL	)) \
	GTA_ONLY(DEF(VEH_TURRET_A_AMMO_BELT	)) \
	GTA_ONLY(DEF(VEH_SUSPENSION_LM1	)) \
	GTA_ONLY(DEF(VEH_SUSPENSION_RM1	)) \
	GTA_ONLY(DEF(VEH_TRANSMISSION_M1	)) \
	GTA_ONLY(DEF(VEH_LEFT_FL	)) \
	GTA_ONLY(DEF(VEH_LEFT_FR	)) \
	GTA_ONLY(DEF(VEH_LEFT_RL	)) \
	GTA_ONLY(DEF(VEH_LEFT_RR	)) \
	GTA_ONLY(DEF(VEH_PANEL_L1	)) \
	GTA_ONLY(DEF(VEH_PANEL_L2	)) \
	GTA_ONLY(DEF(VEH_PANEL_L3	)) \
	GTA_ONLY(DEF(VEH_PANEL_L4	)) \
	GTA_ONLY(DEF(VEH_PANEL_L5	)) \
	GTA_ONLY(DEF(VEH_PANEL_R1	)) \
	GTA_ONLY(DEF(VEH_PANEL_R2	)) \
	GTA_ONLY(DEF(VEH_PANEL_R3	)) \
	GTA_ONLY(DEF(VEH_PANEL_R4	)) \
	GTA_ONLY(DEF(VEH_PANEL_R5	)) \
	GTA_ONLY(DEF(VEH_FOLDING_WING_L	)) \
	GTA_ONLY(DEF(VEH_FOLDING_WING_R	)) \
	GTA_ONLY(DEF(VEH_STAND	)) \
	GTA_ONLY(DEF(VEH_SPEAKER )) \
	GTA_ONLY(DEF(VEH_NOZZLE_F )) \
	GTA_ONLY(DEF(VEH_NOZZLE_R )) \
	GTA_ONLY( DEF( VEH_BLADE_R_1_MOD ) ) \
	GTA_ONLY( DEF( VEH_BLADE_R_1_FAST ) ) \
	GTA_ONLY( DEF( VEH_BLADE_R_2_MOD ) ) \
	GTA_ONLY( DEF( VEH_BLADE_R_2_FAST ) ) \
	GTA_ONLY( DEF( VEH_BLADE_R_3_MOD ) ) \
	GTA_ONLY( DEF( VEH_BLADE_R_3_FAST ) ) \
	GTA_ONLY( DEF( VEH_BLADE_F_1_MOD ) ) \
	GTA_ONLY( DEF( VEH_BLADE_F_1_FAST ) ) \
	GTA_ONLY( DEF( VEH_BLADE_F_2_MOD ) ) \
	GTA_ONLY( DEF( VEH_BLADE_F_2_FAST ) ) \
	GTA_ONLY( DEF( VEH_BLADE_F_3_MOD ) ) \
	GTA_ONLY( DEF( VEH_BLADE_F_3_FAST ) ) \
	GTA_ONLY( DEF( VEH_BLADE_S_1_MOD ) ) \
	GTA_ONLY( DEF( VEH_BLADE_S_1_L_FAST ) ) \
	GTA_ONLY( DEF( VEH_BLADE_S_1_R_FAST ) ) \
	GTA_ONLY( DEF( VEH_BLADE_S_2_MOD ) ) \
	GTA_ONLY( DEF( VEH_BLADE_S_2_L_FAST ) ) \
	GTA_ONLY( DEF( VEH_BLADE_S_2_R_FAST ) ) \
	GTA_ONLY( DEF( VEH_BLADE_S_3_MOD ) ) \
	GTA_ONLY( DEF( VEH_BLADE_S_3_L_FAST ) ) \
	GTA_ONLY( DEF( VEH_BLADE_S_3_R_FAST ) ) \
	GTA_ONLY( DEF( VEH_SPIKE_1_MOD ) ) \
	GTA_ONLY( DEF( VEH_SPIKE_1_PED ) ) \
	GTA_ONLY( DEF( VEH_SPIKE_1_CAR ) ) \
	GTA_ONLY( DEF( VEH_SPIKE_2_MOD ) ) \
	GTA_ONLY( DEF( VEH_SPIKE_2_PED ) ) \
	GTA_ONLY( DEF( VEH_SPIKE_2_CAR ) ) \
	GTA_ONLY( DEF( VEH_SPIKE_3_MOD ) ) \
	GTA_ONLY( DEF( VEH_SPIKE_3_PED ) ) \
	GTA_ONLY( DEF( VEH_SPIKE_3_CAR ) ) \
	GTA_ONLY( DEF( VEH_SCOOP_1_MOD ) ) \
	GTA_ONLY( DEF( VEH_SCOOP_2_MOD ) ) \
	GTA_ONLY( DEF( VEH_SCOOP_3_MOD ) ) \
	GTA_ONLY( DEF( VEH_RAMP_1_MOD ) ) \
	GTA_ONLY( DEF( VEH_RAMP_2_MOD ) ) \
	GTA_ONLY( DEF( VEH_RAMP_3_MOD ) ) \
	GTA_ONLY( DEF( VEH_FRONT_SPIKE_1 ) ) \
	GTA_ONLY( DEF( VEH_FRONT_SPIKE_2 ) ) \
	GTA_ONLY( DEF( VEH_FRONT_SPIKE_3 ) ) \
	GTA_ONLY( DEF( VEH_RAMMING_BAR_1 ) ) \
	GTA_ONLY( DEF( VEH_RAMMING_BAR_2 ) ) \
	GTA_ONLY( DEF( VEH_RAMMING_BAR_3 ) ) \
	GTA_ONLY( DEF( VEH_RAMMING_BAR_4 ) ) \
	GTA_ONLY( DEF( VEH_BOBBLE_MISC_1 ) ) \
	GTA_ONLY( DEF( VEH_BOBBLE_MISC_2 ) ) \
	GTA_ONLY( DEF( VEH_BOBBLE_MISC_3 ) ) \
	GTA_ONLY( DEF( VEH_BOBBLE_MISC_4 ) ) \
	GTA_ONLY( DEF( VEH_BOBBLE_MISC_5 ) ) \
	GTA_ONLY( DEF( VEH_BOBBLE_MISC_6 ) ) \
	GTA_ONLY( DEF( VEH_BOBBLE_MISC_7 ) ) \
	GTA_ONLY( DEF( VEH_BOBBLE_MISC_8 ) ) \
	GTA_ONLY( DEF( VEH_SUPERCHARGER_1 ) ) \
	GTA_ONLY( DEF( VEH_SUPERCHARGER_2 ) ) \
	GTA_ONLY( DEF( VEH_SUPERCHARGER_3 ) ) \
	GTA_ONLY( DEF( VEH_TOMBSTONE ) ) \
	
	// end.

	/*static bool bCheckOnce = true;

	if (bCheckOnce)
	{
		bCheckOnce = false;
		int i = 0;
		#define DEF_CheckVehicleHierarchyId(id) Assertf(i == id, STRING(id)" expected to be %d, but is %d!", i, id); i++;
		FOREACH(DEF_CheckVehicleHierarchyId)
		#undef  DEF_CheckVehicleHierarchyId
	}*/

	static const char* vehHierarchyIdNames[] =
	{
		#define DEF_CheckVehicleHierarchyId(id) STRING(id),
		FOREACH(DEF_CheckVehicleHierarchyId)
		#undef  DEF_CheckVehicleHierarchyId
	};
	CompileTimeAssert(NELEM(vehHierarchyIdNames) == VEH_NUM_NODES);

	return vehHierarchyIdNames;
}

const char* GetVehicleHierarchyIdName(int hierarchyId)
{
	if (hierarchyId >= 0 && hierarchyId < VEH_NUM_NODES)
	{
		return GetVehicleHierarchyIdNames()[hierarchyId];
	}

	return "$UNKNOWN";
}

#if __DEV
#if !RSG_ORBIS
static void VehicleGlassCrackTest(const CVehicle* pVehicle, int geomId, eHierarchyId hierarchyId, int componentId, int boneIndex, Mat34V_In boneMatrix, const CVehicleGlassTriangle* triangles, int numTriangles)
{
	(void)pVehicle;
	(void)geomId;
	(void)hierarchyId;
	(void)componentId;
	(void)boneIndex;
	(void)boneMatrix;
	(void)triangles;
	(void)numTriangles;

	const int framesToLive = 10; // draw the triangles for a few frames

	if (1) // reference code to show how to get at the vertex data
	{
		Displayf("VehicleGlassCrackTest:");
		Displayf("  model = %s", pVehicle->GetModelName());
		Displayf("  geomId = %d", geomId);
		Displayf("  hierarchyId = %d", (int)hierarchyId);
		Displayf("  componentId = %d", componentId);
		Displayf("  boneIndex = %d", boneIndex);
		Displayf("  boneMatrix.pos= %f,%f,%f", VEC3V_ARGS(boneMatrix.GetCol3()));
		Displayf("  numTriangles = %d", numTriangles);

		const ScalarV offset(0.025f);

		for (int pass = 0; pass < 2; pass++)
		{
			for (int i = 0; i < numTriangles; i++)
			{
				const Vec3V P0 = Transform(boneMatrix, triangles[i].GetVertexP(0)) + Transform3x3(boneMatrix, triangles[i].GetVertexN(0))*offset;
				const Vec3V P1 = Transform(boneMatrix, triangles[i].GetVertexP(1)) + Transform3x3(boneMatrix, triangles[i].GetVertexN(1))*offset;
				const Vec3V P2 = Transform(boneMatrix, triangles[i].GetVertexP(2)) + Transform3x3(boneMatrix, triangles[i].GetVertexN(2))*offset;

				grcDebugDraw::Poly(P0, P1, P2, Color32(255,255,0,pass?255:32), true, pass == 0, framesToLive);
			}
		}
	}

	if (1)
	{
		// <-- ADD CODE HERE -->
	}
}
#endif // !RSG_ORBIS

bool CVehicleGlassComponent::ProcessVehicleGlassCrackTestComponent(const VfxCollInfo_s& info)
{
	if (IsEqualAll(info.vNormalWld   , Vec3V(V_ZERO)) != 0 &&
		IsEqualAll(info.vDirectionWld, Vec3V(V_ZERO)) != 0 &&
		info.force == 1.0f)
	{
		CPhysical* pPhysical = m_regdPhy.Get();

		if (pPhysical)
		{
			const bool tessellationEnabledPrev = g_vehicleGlassMan.m_tessellationEnabled;

			// no tessellation
			g_vehicleGlassMan.m_tessellationEnabled = false;

			ProcessGroupCollision(info, false, true);

			Mat34V boneMatrix;
			CVfxHelper::GetMatrixFromBoneIndex_Smash(boneMatrix, pPhysical, m_boneIndex);

			// allocate temporary storage for triangles
			/*const int numTriangles = GetNumTriangles(true);
			CVehicleGlassTriangle* triangles = rage_new CVehicleGlassTriangle[numTriangles];
			int i = 0;

			for (const CVehicleGlassTriangle* pTriangle = m_triangleList.GetHead(); pTriangle; pTriangle = m_triangleList.GetNext(pTriangle))
			{
				triangles[i++] = *pTriangle;
			}

			VehicleGlassCrackTest(
				static_cast<const CVehicle*>(pPhysical), // it's always a vehicle
				m_geomIndex,
				m_hierarchyId,
				m_componentId,
				m_boneIndex,
				boneMatrix,
				triangles,
				numTriangles
			);

			// we're done
			delete[] triangles;*/

			pPhysical->ClearBrokenFlag(m_componentId);

			g_vehicleGlassMan.m_tessellationEnabled = tessellationEnabledPrev; // restore
		}

		return true;
	}

	return false;
}

#endif // __DEV

// ================================================================================================

#if VEHICLE_GLASS_SMASH_TEST

CompileTimeAssert(__DEV);

// rage
#include "diag/art_channel.h"
#include "math/vecshift.h" // for BIT64
#include "physics/gtaInst.h"
#include "string/stringutil.h"
#include "system/memops.h"
#include "system/xtl.h"

// framework
#include "fwtl/customvertexpushbuffer.h"
#include "streaming/defragmentation.h"
#include "vfx/channel.h"
#include "vfx/vfxwidget.h"

// game
#include "camera/viewports/ViewportManager.h"
#include "Core/game.h"
#include "crskeleton/skeleton.h"
#include "debug/DebugGeometryUtil.h"
#include "debug/DebugGlobals.h"
#include "modelinfo/modelinfo_channel.h"
#include "Network/Objects/Entities/NetObjVehicle.h"
#include "Objects/object.h"
#include "Peds/ped.h"
#include "physics/gtaMaterialDebug.h"
#include "physics/physics.h"
#include "renderer/Lights/lights.h"
#include "renderer/RenderPhases/RenderPhaseDebugOverlay.h"
#include "renderer/RenderPhases/RenderPhaseReflection.h"
#include "scene/world/GameWorld.h"
#include "Vehicles/VehicleFactory.h"
#include "vfx/Particles/PtFxCollision.h"
#include "vfx/vehicleglass/VehicleGlassManager.h"
#include "vfx/vehicleglass/VehicleGlassSmashTest.h"

VEHICLE_GLASS_OPTIMISATIONS()

CVehicleGlassSmashTestManager g_vehicleGlassSmashTest;

XPARAM(level);

void CVehicleGlassSmashTestManager::Init()
{
	const char* pLevel = NULL;

	if (!PARAM_level.Get(pLevel))
	{
		pLevel = RS_PROJECT;
	}

	if (stricmp(pLevel, RS_PROJECT) == 0)
	{
		Assertf(0, "### VEHICLE_GLASS_SMASH_TEST is defined while loading main level (%s), there might not be enough memory for this", pLevel);
	}

	m_smashTest                        = false;
	m_smashTestSimple                  = false;
	m_smashTestOutputWindowFlagsOnly   = false;
	m_smashTestOutputOBJsOnly          = false;
	m_smashTestConstructWindows        = false;
	m_smashTestAuto                    = false;
	m_smashTestAutoTime                = 0;
	m_smashTestAutoTimeStep            = 1.0f;
	m_smashTestAutoRotation            = 0.0f;
	m_smashTestAllGlassIsWeak          = true;
	m_smashTestAcuteAngleCheck         = false;
	m_smashTestTexcoordBoundsCheckOnly = false;
	m_smashTestBoneNameMaterialCheck   = false;
	m_smashTestUnexpectedHierarchyIds  = false;
	m_smashTestCompactOutput           = true;
	m_smashTestDumpGeometryToFile      = false;
	m_smashTestReportOk                = false;

	m_smashTestReportSmashableGlassOnAnyNonSmashableMaterials = false;
	m_smashTestReportSmashableGlassOnAllNonSmashableMaterials = false;
}

void CVehicleGlassSmashTestManager::InitWidgets(bkBank* pVfxBank)
{
	pVfxBank->PushGroup("Smash Test", false);
	{
		class SmashTest_changed { public: static void func()
		{
			gbAllowVehGenerationOrRemoval   = g_vehicleGlassSmashTest.m_smashTest;
			gbAllowTrainGenerationOrRemoval = g_vehicleGlassSmashTest.m_smashTest;
		}};
		pVfxBank->AddToggle("Smash Test"                            , &m_smashTest, SmashTest_changed::func);
		pVfxBank->AddToggle("Smash Test Simple"                     , &m_smashTestSimple);
		pVfxBank->AddToggle("Smash Test Output Window Flags Only"   , &m_smashTestOutputWindowFlagsOnly);
		pVfxBank->AddToggle("Smash Test Output OBJs Only"           , &m_smashTestOutputOBJsOnly);
		pVfxBank->AddToggle("Smash Test Construct Windows"          , &m_smashTestConstructWindows);
		pVfxBank->AddToggle("Smash Test Auto"                       , &m_smashTestAuto);
		pVfxBank->AddSlider("Smash Test Auto Time Step"             , &m_smashTestAutoTimeStep, 0.0f, 10.0f, 1.0f/32.0f);
		pVfxBank->AddSlider("Smash Test Auto Rotation"              , &m_smashTestAutoRotation, 0.0f, 360.0f, 1.0f/32.0f);
		pVfxBank->AddToggle("Smash Test All Glass Is Weak"          , &m_smashTestAllGlassIsWeak);
		pVfxBank->AddToggle("Smash Test Acute Angle Check"          , &m_smashTestAcuteAngleCheck);
		pVfxBank->AddToggle("Smash Test Texcoord Bounds Check Only" , &m_smashTestTexcoordBoundsCheckOnly);
		pVfxBank->AddToggle("Smash Test Bone Name Material Check"   , &m_smashTestBoneNameMaterialCheck);
		pVfxBank->AddToggle("Smash Test Unexpected Hierarchy IDs"   , &m_smashTestUnexpectedHierarchyIds);
		pVfxBank->AddToggle("Smash Test Compact Output"             , &m_smashTestCompactOutput);
		pVfxBank->AddToggle("Smash Test Dump Geometry"              , &m_smashTestDumpGeometryToFile);
		pVfxBank->AddToggle("Smash Test Report OK"                  , &m_smashTestReportOk);

		pVfxBank->AddToggle("Smash Test Report Smashable Glass On Any Non-Smashable Materials", &m_smashTestReportSmashableGlassOnAnyNonSmashableMaterials);
		pVfxBank->AddToggle("Smash Test Report Smashable Glass On All Non-Smashable Materials", &m_smashTestReportSmashableGlassOnAllNonSmashableMaterials);
	}
	pVfxBank->PopGroup();
}

void CVehicleGlassSmashTestManager::StoreCollision(const VfxCollInfo_s& info, int smashTestModelIndex)
{
	for (int i = 0; i < m_smashTests.GetCount(); i++)
	{
		if (m_smashTests[i].m_modelIndex == smashTestModelIndex)
		{
			m_smashTests[i].m_collisions.PushAndGrow(info);
			return;
		}
	}

	m_smashTests.PushAndGrow(CSmashTest(smashTestModelIndex, info));
}

void CVehicleGlassSmashTestManager::UpdateSafe()
{
	SmashTestUpdate();

	for (int i = 0; i < m_smashTests.GetCount(); i++)
	{
		m_smashTests[i].Update();
	}

	m_smashTests.clear();
}

void CVehicleGlassSmashTestManager::UpdateDebug()
{
	if (m_smashTestAuto)
	{
		CVehicleFactory::CreateBank();

		//static int frame = 0;
		//if (++frame%30 == 0)
		if (fwTimer::GetSystemTimeInMilliseconds() >= m_smashTestAutoTime + (u32)(m_smashTestAutoTimeStep*1000.0f))
		{
			m_smashTestAutoTime = fwTimer::GetSystemTimeInMilliseconds();
			extern void CreateNextCarCB();
			CreateNextCarCB();
		}
	}
}

static const phBound* GetMaterialIdsFromComponentId(atArray<phMaterialMgr::Id>& mtlIds, const CPhysical* pPhysical, int componentId)
{
	const phArchetype* pArchetype = pPhysical->GetFragInst()->GetArchetype();

	if (AssertVerify(pArchetype))
	{
		const phBound* pBound = pArchetype->GetBound();

		if (AssertVerify(pBound->GetType() == phBound::COMPOSITE))
		{
			const phBound* pBoundChild = static_cast<const phBoundComposite*>(pBound)->GetBound(componentId);

			// not sure which of these conditions need to be tested .. 
			// we can probably remove these checks from 'GetSmashableGlassMaterialId'

		//	if (Verifyf(pBoundChild, "%s: componentId=%d, pBoundChild is NULL", pPhysical->GetModelName(), componentId))
		//	if (Verifyf(pBoundChild->GetType() == phBound::GEOMETRY, "%s: componentId=%d, pBoundChild type is %d", pPhysical->GetModelName(), componentId, pBoundChild->GetType()))
		//	if (Verifyf(pBoundChild->GetNumMaterials() == 1, "%s: componentId=%d, pBoundChild has %d materials", pPhysical->GetModelName(), componentId, pBoundChild->GetNumMaterials()))

			if (pBoundChild)
			{
				for (int i = 0; i < pBoundChild->GetNumMaterials(); i++)
				{
					mtlIds.PushAndGrow(pBoundChild->GetMaterialId(i));
				}
			}

			return pBoundChild;
		}
	}

	return NULL;
}

// ================================================================================================
/*
still need to check:

- vehicle_vehglass outside and vehicle_vehglass_inner inside
	- try automatic rotation while smash testing, with no detaching, and hack the shaders

- easy to check:
	if it has >0 smashable components, it should have both vehicle_vehglass and vehicle_vehglass_inner in the drawable
	else, it should have neither of these shaders in the drawable

- physics materials don't "cover up" the vehicle glass
	- make a new physics debug mode that shows all non-vehicle glass materials, and hack the vehicle glass shaders to not write to depth
*/

/*
smash test results (compact output):
===========================================
TOWTRUCK (VEHICLE_TYPE_CAR) ............ no smashable glass groups created for VEH_SIREN_GLASS_2
TOWTRUCK (VEHICLE_TYPE_CAR) ............ no smashable glass groups created for VEH_SIREN_GLASS_1
TOWTRUCK (VEHICLE_TYPE_CAR) ............ multiple glass geometries in [VEH_SIREN_GLASS_2,VEH_SIREN_GLASS_1], 4 ok
banshee (VEHICLE_TYPE_CAR) ............. found 46 smashable glass triangles attached to non-smashable bone 'windscreen_r' material [NULL]
manana (VEHICLE_TYPE_CAR) .............. found 12 smashable glass triangles attached to non-smashable bone 'windscreen_r' material [NULL]
manana (VEHICLE_TYPE_CAR) .............. found 6 smashable glass triangles attached to non-smashable bone 'window_rr' material [NULL]
manana (VEHICLE_TYPE_CAR) .............. found 6 smashable glass triangles attached to non-smashable bone 'window_lr' material [NULL]
Pounder (VEHICLE_TYPE_CAR) ............. no smashable glass groups created for VEH_WINDSCREEN
Pounder (VEHICLE_TYPE_CAR) ............. multiple glass geometries in [VEH_WINDSCREEN], 4 ok
seraph2 (VEHICLE_TYPE_CAR) ............. no smashable glass groups created for VEH_WINDOW_LR
seraph2 (VEHICLE_TYPE_CAR) ............. no smashable glass groups created for VEH_WINDOW_RR
seraph2 (VEHICLE_TYPE_CAR) ............. no smashable glass groups created for VEH_WINDOW_LF
seraph2 (VEHICLE_TYPE_CAR) ............. no smashable glass groups created for VEH_WINDOW_RF
seraph2 (VEHICLE_TYPE_CAR) ............. found 102 smashable glass triangles attached to non-smashable bone 'window_lf_2' material []
seraph2 (VEHICLE_TYPE_CAR) ............. found 102 smashable glass triangles attached to non-smashable bone 'window_rf_2' material []
seraph2 (VEHICLE_TYPE_CAR) ............. found 54 smashable glass triangles attached to non-smashable bone 'window_lr_2' material []
seraph2 (VEHICLE_TYPE_CAR) ............. found 54 smashable glass triangles attached to non-smashable bone 'window_rr_2' material []
seraph2 (VEHICLE_TYPE_CAR) ............. multiple glass geometries in [VEH_WINDOW_LR,VEH_WINDOW_RR,VEH_WINDOW_LF,VEH_WINDOW_RF], 2 ok
stinger (VEHICLE_TYPE_CAR) ............. found 42 smashable glass triangles attached to non-smashable bone 'windscreen_r' material []
stratum (VEHICLE_TYPE_CAR) ............. unexpected glass hierarchyId in [VEH_EXTRA_1], 8 ok
submersible (VEHICLE_TYPE_SUBMARINE) ... multiple glass geometries in [VEH_WINDSCREEN], 0 ok

reporting smashable glass on non-smashable materials:
TipTruck2 (VEHICLE_TYPE_CAR) ........... found 200 smashable glass triangles attached to non-smashable bone 'chassis' material [car_metal,phys_car_void,car_metal,car_metal,car_metal,car_metal,car_metal,car_metal,car_metal,car_metal,car_metal]
titan (VEHICLE_TYPE_PLANE) ............. found 24 smashable glass triangles attached to non-smashable bone 'chassis' material [car_metal,car_metal,car_plastic,phys_car_void]
tornado (VEHICLE_TYPE_CAR) ............. found 320 smashable glass triangles attached to non-smashable bone 'bodyshell' material []
tornado (VEHICLE_TYPE_CAR) ............. found 256 smashable glass triangles attached to non-smashable bone 'bumper_r' material [car_metal]
tornado (VEHICLE_TYPE_CAR) ............. found 464 smashable glass triangles attached to non-smashable bone 'wing_lf' material [car_metal]
tornado (VEHICLE_TYPE_CAR) ............. found 464 smashable glass triangles attached to non-smashable bone 'wing_rf' material [car_metal]
tornado2 (VEHICLE_TYPE_CAR) ............ found 84 smashable glass triangles attached to non-smashable bone 'soft_2' material []
tornado2 (VEHICLE_TYPE_CAR) ............ found 280 smashable glass triangles attached to non-smashable bone 'bodyshell' material []
tornado2 (VEHICLE_TYPE_CAR) ............ found 224 smashable glass triangles attached to non-smashable bone 'bumper_r' material [car_metal]
tornado2 (VEHICLE_TYPE_CAR) ............ found 406 smashable glass triangles attached to non-smashable bone 'wing_lf' material [car_metal]
tornado2 (VEHICLE_TYPE_CAR) ............ found 406 smashable glass triangles attached to non-smashable bone 'wing_rf' material [car_metal]
tornado3 (VEHICLE_TYPE_CAR) ............ found 320 smashable glass triangles attached to non-smashable bone 'bodyshell' material []
tornado3 (VEHICLE_TYPE_CAR) ............ found 320 smashable glass triangles attached to non-smashable bone 'bumper_r' material [car_metal]
tornado3 (VEHICLE_TYPE_CAR) ............ found 512 smashable glass triangles attached to non-smashable bone 'wing_lf' material [car_metal]
tornado3 (VEHICLE_TYPE_CAR) ............ found 512 smashable glass triangles attached to non-smashable bone 'wing_rf' material [car_metal]
tornado4 (VEHICLE_TYPE_CAR) ............ found 228 smashable glass triangles attached to non-smashable bone 'wing_lf' material [car_metal]
tornado4 (VEHICLE_TYPE_CAR) ............ found 228 smashable glass triangles attached to non-smashable bone 'wing_rf' material [car_metal]
tornado4 (VEHICLE_TYPE_CAR) ............ found 120 smashable glass triangles attached to non-smashable bone 'bumper_r' material [car_metal]
tornado4 (VEHICLE_TYPE_CAR) ............ found 72 smashable glass triangles attached to non-smashable bone 'bodyshell' material []
TOWTRUCK (VEHICLE_TYPE_CAR) ............ no smashable glass groups created for VEH_SIREN_GLASS_2
TOWTRUCK (VEHICLE_TYPE_CAR) ............ no smashable glass groups created for VEH_SIREN_GLASS_1
TOWTRUCK (VEHICLE_TYPE_CAR) ............ found 540 smashable glass triangles attached to non-smashable bone 'chassis' material [car_metal,car_metal,phys_car_void,car_metal,phys_car_void]
TOWTRUCK (VEHICLE_TYPE_CAR) ............ multiple glass geometries in [VEH_SIREN_GLASS_2,VEH_SIREN_GLASS_1], 4 ok
Towtruck2 (VEHICLE_TYPE_CAR) ........... found 144 smashable glass triangles attached to non-smashable bone 'misc_c' material []
Towtruck2 (VEHICLE_TYPE_CAR) ........... found 144 smashable glass triangles attached to non-smashable bone 'misc_d' material []
tropic (VEHICLE_TYPE_BOAT) ............. found 60 smashable glass triangles attached to non-smashable bone 'misc_b' material [NULL]
tropic (VEHICLE_TYPE_BOAT) ............. found 108 smashable glass triangles attached to non-smashable bone 'misc_a' material [NULL]
tropic (VEHICLE_TYPE_BOAT) ............. found 36 smashable glass triangles attached to non-smashable bone 'misc_d' material [NULL]
tropic (VEHICLE_TYPE_BOAT) ............. found 60 smashable glass triangles attached to non-smashable bone 'misc_c' material [NULL]
Utillitruck3 (VEHICLE_TYPE_CAR) ........ found 176 smashable glass triangles attached to non-smashable bone 'extra_3' material []
voodoo2 (VEHICLE_TYPE_CAR) ............. found 56 smashable glass triangles attached to non-smashable bone 'door_dside_f' material [car_metal]
voodoo2 (VEHICLE_TYPE_CAR) ............. found 56 smashable glass triangles attached to non-smashable bone 'wing_lf' material [car_metal]
voodoo2 (VEHICLE_TYPE_CAR) ............. found 21 smashable glass triangles attached to non-smashable bone 'misc_b' material [car_metal]
voodoo2 (VEHICLE_TYPE_CAR) ............. found 56 smashable glass triangles attached to non-smashable bone 'bodyshell' material []
vulkan (VEHICLE_TYPE_PLANE) ............ found 56 smashable glass triangles attached to non-smashable bone 'bodyshell' material []
vulkan (VEHICLE_TYPE_PLANE) ............ found 14 smashable glass triangles attached to non-smashable bone 'wing_l' material [car_metal,car_metal,car_metal,default]
vulkan (VEHICLE_TYPE_PLANE) ............ found 14 smashable glass triangles attached to non-smashable bone 'wing_r' material [car_metal,car_metal,car_metal,default]
AMBULANCE (VEHICLE_TYPE_CAR) ........... found 180 smashable glass triangles attached to non-smashable bone 'misc_d' material []
AMBULANCE (VEHICLE_TYPE_CAR) ........... found 660 smashable glass triangles attached to non-smashable bone 'bodyshell' material []
Baller (VEHICLE_TYPE_CAR) .............. found 128 smashable glass triangles attached to non-smashable bone 'bodyshell' material []
Baller (VEHICLE_TYPE_CAR) .............. found 160 smashable glass triangles attached to non-smashable bone 'bumper_f' material [car_metal]
bati (VEHICLE_TYPE_BIKE) ............... found 42 smashable glass triangles attached to non-smashable bone 'chassis' material [car_metal,car_metal]
blista (VEHICLE_TYPE_CAR) .............. found 540 smashable glass triangles attached to non-smashable bone 'chassis' material [car_metal,phys_car_void]
bobcatxl (VEHICLE_TYPE_CAR) ............ found 12 smashable glass triangles attached to non-smashable bone 'misc_c' material [NULL]
bobcatxl (VEHICLE_TYPE_CAR) ............ found 24 smashable glass triangles attached to non-smashable bone 'misc_a' material [NULL]
bobcatxl (VEHICLE_TYPE_CAR) ............ found 12 smashable glass triangles attached to non-smashable bone 'misc_b' material [NULL]
Bodhi2 (VEHICLE_TYPE_CAR) .............. found 4 smashable glass triangles attached to non-smashable bone 'chassis' material [phys_car_void,phys_car_void,car_metal,car_metal,car_metal,car_metal,car_metal,car_metal,car_metal,car_metal,car_metal,car_metal,car_metal,car_metal,car_metal,car_metal]
boxville (VEHICLE_TYPE_CAR) ............ found 80 smashable glass triangles attached to non-smashable bone 'siren_glass' material []
boxville (VEHICLE_TYPE_CAR) ............ found 80 smashable glass triangles attached to non-smashable bone 'siren_glass2' material []
boxville (VEHICLE_TYPE_CAR) ............ found 80 smashable glass triangles attached to non-smashable bone 'siren_glass3' material []
boxville (VEHICLE_TYPE_CAR) ............ found 80 smashable glass triangles attached to non-smashable bone 'siren_glass4' material []
bulldozer (VEHICLE_TYPE_CAR) ........... found 140 smashable glass triangles attached to non-smashable bone 'bodyshell' material []
Burrito (VEHICLE_TYPE_CAR) ............. found 48 smashable glass triangles attached to non-smashable bone 'bodyshell' material []
buzzard (VEHICLE_TYPE_HELI) ............ found 24 smashable glass triangles attached to non-smashable bone 'weapon_3a' material []
Cargobob (VEHICLE_TYPE_HELI) ........... found 2184 smashable glass triangles attached to non-smashable bone 'rotor_main_slow' material []
Cargobob (VEHICLE_TYPE_HELI) ........... found 2184 smashable glass triangles attached to non-smashable bone 'rotor_rear_slow' material []
cavalcade (VEHICLE_TYPE_CAR) ........... found 64 smashable glass triangles attached to non-smashable bone 'bodyshell' material []
coach (VEHICLE_TYPE_CAR) ............... found 228 smashable glass triangles attached to non-smashable bone 'bumper_f' material [car_metal]
Emperor2 (VEHICLE_TYPE_CAR) ............ found 8 smashable glass triangles attached to non-smashable bone 'chassis' material [car_metal,phys_car_void]
emperor3 (VEHICLE_TYPE_CAR) ............ found 8 smashable glass triangles attached to non-smashable bone 'chassis' material [car_metal,phys_car_void]
entityxf (VEHICLE_TYPE_CAR) ............ found 60 smashable glass triangles attached to non-smashable bone 'chassis' material [car_metal,phys_car_void]
entityxf (VEHICLE_TYPE_CAR) ............ found 54 smashable glass triangles attached to non-smashable bone 'bumper_r' material [car_plastic]
entityxf (VEHICLE_TYPE_CAR) ............ found 168 smashable glass triangles attached to non-smashable bone 'bumper_f' material [car_plastic]
entityxf (VEHICLE_TYPE_CAR) ............ found 624 smashable glass triangles attached to non-smashable bone 'boot' material [car_metal]
entityxf (VEHICLE_TYPE_CAR) ............ found 168 smashable glass triangles attached to non-smashable bone 'bodyshell' material []
felon2 (VEHICLE_TYPE_CAR) .............. found 150 smashable glass triangles attached to non-smashable bone 'soft_5' material []
firetruk (VEHICLE_TYPE_CAR) ............ found 256 smashable glass triangles attached to non-smashable bone 'bumper_f' material [car_metal]
firetruk (VEHICLE_TYPE_CAR) ............ found 64 smashable glass triangles attached to non-smashable bone 'bumper_r' material [car_metal]
firetruk (VEHICLE_TYPE_CAR) ............ found 1152 smashable glass triangles attached to non-smashable bone 'bodyshell' material []
freight (VEHICLE_TYPE_TRAIN) ........... found 16 smashable glass triangles attached to non-smashable bone 'door_pside_f' material [phys_car_void,car_metal]
freight (VEHICLE_TYPE_TRAIN) ........... found 2640 smashable glass triangles attached to non-smashable bone 'bodyshell' material []
gburrito (VEHICLE_TYPE_CAR) ............ found 40 smashable glass triangles attached to non-smashable bone 'bumper_r' material [car_metal]
gburrito (VEHICLE_TYPE_CAR) ............ found 40 smashable glass triangles attached to non-smashable bone 'bumper_f' material [car_metal]
hunter (VEHICLE_TYPE_HELI) ............. found 231 smashable glass triangles attached to non-smashable bone 'bodyshell' material []
infernus (VEHICLE_TYPE_CAR) ............ found 12 smashable glass triangles attached to non-smashable bone 'chassis' material [car_metal,phys_car_void]
infernus (VEHICLE_TYPE_CAR) ............ found 102 smashable glass triangles attached to non-smashable bone 'bumper_f' material []
infernus (VEHICLE_TYPE_CAR) ............ found 48 smashable glass triangles attached to non-smashable bone 'bodyshell' material []
intruder (VEHICLE_TYPE_CAR) ............ found 64 smashable glass triangles attached to non-smashable bone 'chassis' material [phys_car_void,car_metal]
intruder (VEHICLE_TYPE_CAR) ............ found 168 smashable glass triangles attached to non-smashable bone 'bumper_f' material [car_plastic]
issi2 (VEHICLE_TYPE_CAR) ............... found 80 smashable glass triangles attached to non-smashable bone 'soft_3' material []
jackal (VEHICLE_TYPE_CAR) .............. found 390 smashable glass triangles attached to non-smashable bone 'chassis' material [car_metal,phys_car_void]
jackal (VEHICLE_TYPE_CAR) .............. found 144 smashable glass triangles attached to non-smashable bone 'boot' material [car_metal]
landstalker (VEHICLE_TYPE_CAR) ......... found 64 smashable glass triangles attached to non-smashable bone 'bumper_f' material [car_metal]
lazer (VEHICLE_TYPE_PLANE) ............. found 8 smashable glass triangles attached to non-smashable bone 'rudder_2' material [car_metal]
lazer (VEHICLE_TYPE_PLANE) ............. found 8 smashable glass triangles attached to non-smashable bone 'rudder' material [car_metal]
lazer (VEHICLE_TYPE_PLANE) ............. found 4 smashable glass triangles attached to non-smashable bone 'elevator_l' material [car_metal]
lazer (VEHICLE_TYPE_PLANE) ............. found 4 smashable glass triangles attached to non-smashable bone 'elevator_r' material [car_metal]
lazer (VEHICLE_TYPE_PLANE) ............. found 92 smashable glass triangles attached to non-smashable bone 'bodyshell' material []
lazer (VEHICLE_TYPE_PLANE) ............. found 36 smashable glass triangles attached to non-smashable bone 'wing_r' material [car_metal,car_metal,car_metal,car_metal,car_metal]
lazer (VEHICLE_TYPE_PLANE) ............. found 36 smashable glass triangles attached to non-smashable bone 'wing_l' material [car_metal,car_metal,car_metal,car_metal,car_metal]
mammatus (VEHICLE_TYPE_PLANE) .......... found 60 smashable glass triangles attached to non-smashable bone 'wing_l' material [car_metal,car_metal]
mammatus (VEHICLE_TYPE_PLANE) .......... found 60 smashable glass triangles attached to non-smashable bone 'wing_r' material [car_metal,car_metal]
manana (VEHICLE_TYPE_CAR) .............. found 12 smashable glass triangles attached to non-smashable bone 'windscreen_r' material [NULL]
manana (VEHICLE_TYPE_CAR) .............. found 6 smashable glass triangles attached to non-smashable bone 'window_rr' material [NULL]
manana (VEHICLE_TYPE_CAR) .............. found 6 smashable glass triangles attached to non-smashable bone 'window_lr' material [NULL]
maverick (VEHICLE_TYPE_HELI) ........... found 64 smashable glass triangles attached to non-smashable bone 'chassis' material [car_metal,phys_car_void,default]
Mesa (VEHICLE_TYPE_CAR) ................ found 60 smashable glass triangles attached to non-smashable bone 'misc_a' material [NULL]
Mesa (VEHICLE_TYPE_CAR) ................ found 60 smashable glass triangles attached to non-smashable bone 'misc_b' material [NULL]
Mesa (VEHICLE_TYPE_CAR) ................ found 60 smashable glass triangles attached to non-smashable bone 'windscreen_r' material [NULL]
ninef2 (VEHICLE_TYPE_CAR) .............. found 12 smashable glass triangles attached to non-smashable bone 'soft_5' material []
oracle (VEHICLE_TYPE_CAR) .............. found 32 smashable glass triangles attached to non-smashable bone 'bumper_f' material [car_plastic]
peyote (VEHICLE_TYPE_CAR) .............. found 8 smashable glass triangles attached to non-smashable bone 'windscreen_r' material [NULL]
police3 (VEHICLE_TYPE_CAR) ............. found 128 smashable glass triangles attached to non-smashable bone 'chassis' material [car_metal,phys_car_void,car_metal]
police3 (VEHICLE_TYPE_CAR) ............. found 832 smashable glass triangles attached to non-smashable bone 'misc_a' material [car_metal,phys_car_void,car_metal]
police3 (VEHICLE_TYPE_CAR) ............. found 256 smashable glass triangles attached to non-smashable bone 'bodyshell' material []
policeb (VEHICLE_TYPE_BIKE) ............ found 4 smashable glass triangles attached to non-smashable bone 'forks_u' material []
policeb (VEHICLE_TYPE_BIKE) ............ found 4 smashable glass triangles attached to non-smashable bone 'forks_l' material []
policeb (VEHICLE_TYPE_BIKE) ............ found 32 smashable glass triangles attached to non-smashable bone 'bodyshell' material []
policet (VEHICLE_TYPE_CAR) ............. found 120 smashable glass triangles attached to non-smashable bone 'siren_glass1' material [car_plastic]
policet (VEHICLE_TYPE_CAR) ............. found 60 smashable glass triangles attached to non-smashable bone 'siren_glass2' material [car_plastic]
policet (VEHICLE_TYPE_CAR) ............. found 120 smashable glass triangles attached to non-smashable bone 'siren_glass3' material [car_plastic]
policet (VEHICLE_TYPE_CAR) ............. found 96 smashable glass triangles attached to non-smashable bone 'bodyshell' material []
Pounder (VEHICLE_TYPE_CAR) ............. no smashable glass groups created for VEH_WINDSCREEN
Pounder (VEHICLE_TYPE_CAR) ............. multiple glass geometries in [VEH_WINDSCREEN], 4 ok
pRanger (VEHICLE_TYPE_CAR) ............. found 640 smashable glass triangles attached to non-smashable bone 'misc_a' material [car_plastic]
premier (VEHICLE_TYPE_CAR) ............. found 96 smashable glass triangles attached to non-smashable bone 'bumper_f' material [car_metal]
primo (VEHICLE_TYPE_CAR) ............... found 64 smashable glass triangles attached to non-smashable bone 'bumper_f' material [car_metal]
proptrailer (VEHICLE_TYPE_TRAILER) ..... found 36 smashable glass triangles attached to non-smashable bone 'misc_c' material [car_metal]
RapidGT2 (VEHICLE_TYPE_CAR) ............ found 72 smashable glass triangles attached to non-smashable bone 'soft_10' material []
RIOT (VEHICLE_TYPE_CAR) ................ found 338 smashable glass triangles attached to non-smashable bone 'extra_1' material []
Ripley (VEHICLE_TYPE_CAR) .............. found 448 smashable glass triangles attached to non-smashable bone 'bodyshell' material []
ruiner (VEHICLE_TYPE_CAR) .............. found 156 smashable glass triangles attached to non-smashable bone 'chassis' material [car_metal,phys_car_void]
ruiner (VEHICLE_TYPE_CAR) .............. found 12 smashable glass triangles attached to non-smashable bone 'wing_lf' material [car_metal]
ruiner (VEHICLE_TYPE_CAR) .............. found 12 smashable glass triangles attached to non-smashable bone 'wing_rf' material [car_metal]
ruiner (VEHICLE_TYPE_CAR) .............. found 36 smashable glass triangles attached to non-smashable bone 'door_dside_f' material [car_metal]
ruiner (VEHICLE_TYPE_CAR) .............. found 36 smashable glass triangles attached to non-smashable bone 'door_pside_f' material [car_metal]
ruiner (VEHICLE_TYPE_CAR) .............. found 24 smashable glass triangles attached to non-smashable bone 'bodyshell' material []
sabregt (VEHICLE_TYPE_CAR) ............. found 48 smashable glass triangles attached to non-smashable bone 'chassis' material [car_metal,phys_car_void]
sabregt (VEHICLE_TYPE_CAR) ............. found 24 smashable glass triangles attached to non-smashable bone 'door_dside_f' material [car_metal]
sabregt (VEHICLE_TYPE_CAR) ............. found 24 smashable glass triangles attached to non-smashable bone 'door_pside_f' material [car_metal]
sabregt (VEHICLE_TYPE_CAR) ............. found 24 smashable glass triangles attached to non-smashable bone 'extra_1' material []
scrap (VEHICLE_TYPE_CAR) ............... found 320 smashable glass triangles attached to non-smashable bone 'chassis' material [car_metal,phys_car_void,car_metal,car_metal,car_metal,car_metal]
seraph2 (VEHICLE_TYPE_CAR) ............. no smashable glass groups created for VEH_WINDOW_LR
seraph2 (VEHICLE_TYPE_CAR) ............. no smashable glass groups created for VEH_WINDOW_RR
seraph2 (VEHICLE_TYPE_CAR) ............. no smashable glass groups created for VEH_WINDOW_LF
seraph2 (VEHICLE_TYPE_CAR) ............. no smashable glass groups created for VEH_WINDOW_RF
seraph2 (VEHICLE_TYPE_CAR) ............. found 102 smashable glass triangles attached to non-smashable bone 'window_lf_2' material []
seraph2 (VEHICLE_TYPE_CAR) ............. found 102 smashable glass triangles attached to non-smashable bone 'window_rf_2' material []
seraph2 (VEHICLE_TYPE_CAR) ............. found 54 smashable glass triangles attached to non-smashable bone 'window_lr_2' material []
seraph2 (VEHICLE_TYPE_CAR) ............. found 54 smashable glass triangles attached to non-smashable bone 'window_rr_2' material []
seraph2 (VEHICLE_TYPE_CAR) ............. multiple glass geometries in [VEH_WINDOW_LR,VEH_WINDOW_RR,VEH_WINDOW_LF,VEH_WINDOW_RF], 2 ok
SQUADDIE (VEHICLE_TYPE_CAR) ............ found 16 smashable glass triangles attached to non-smashable bone 'bonnet' material [car_metal]
stinger (VEHICLE_TYPE_CAR) ............. found 42 smashable glass triangles attached to non-smashable bone 'windscreen_r' material []
stockade (VEHICLE_TYPE_CAR) ............ found 10 smashable glass triangles attached to non-smashable bone 'misc_c' material [default]
stockade (VEHICLE_TYPE_CAR) ............ found 10 smashable glass triangles attached to non-smashable bone 'misc_e' material [default]
stockade (VEHICLE_TYPE_CAR) ............ found 10 smashable glass triangles attached to non-smashable bone 'misc_f' material [default]
stockade (VEHICLE_TYPE_CAR) ............ found 10 smashable glass triangles attached to non-smashable bone 'misc_g' material [default]
stockade2 (VEHICLE_TYPE_CAR) ........... found 40 smashable glass triangles attached to non-smashable bone 'misc_c' material [default]
stockade3 (VEHICLE_TYPE_CAR) ........... found 40 smashable glass triangles attached to non-smashable bone 'misc_c' material [default]
stratum (VEHICLE_TYPE_CAR) ............. found 72 smashable glass triangles attached to non-smashable bone 'bumper_f' material [car_plastic]
stratum (VEHICLE_TYPE_CAR) ............. unexpected glass hierarchyId in [VEH_EXTRA_1], 8 ok
Stunt (VEHICLE_TYPE_PLANE) ............. found 24 smashable glass triangles attached to non-smashable bone 'bodyshell' material []
submersible (VEHICLE_TYPE_SUBMARINE) ... found 16 smashable glass triangles attached to non-smashable bone 'bodyshell' material []
submersible (VEHICLE_TYPE_SUBMARINE) ... multiple glass geometries in [VEH_WINDSCREEN], 0 ok
Surano (VEHICLE_TYPE_CAR) .............. found 72 smashable glass triangles attached to non-smashable bone 'soft_6' material []
Taco (VEHICLE_TYPE_CAR) ................ found 440 smashable glass triangles attached to non-smashable bone 'chassis' material [car_metal,phys_car_void]
Taco (VEHICLE_TYPE_CAR) ................ found 440 smashable glass triangles attached to non-smashable bone 'boot' material [car_metal,phys_car_void]

vehicle hierarchy IDs which are used:
  VEH_CHASSIS (226)
  VEH_CHASSIS_LOWLOD (163)
  VEH_DOOR_DSIDE_F (206)
  VEH_DOOR_DSIDE_R (97)
  VEH_DOOR_PSIDE_F (197)
  VEH_DOOR_PSIDE_R (98)
  VEH_HANDLE_DSIDE_F (166)
  VEH_HANDLE_DSIDE_R (74)
  VEH_HANDLE_PSIDE_F (161)
  VEH_HANDLE_PSIDE_R (73)
  VEH_WHEEL_LF (225)
  VEH_WHEEL_RF (201)
  VEH_WHEEL_LR (222)
  VEH_WHEEL_RR (213)
  VEH_WHEEL_LM1 (28)
  VEH_WHEEL_RM1 (21)
  VEH_WHEEL_LM2 (4)
  VEH_WHEEL_RM2 (4)
  VEH_WHEEL_LM3 (2)
  VEH_WHEEL_RM3 (2)
  VEH_SUSPENSION_LF (192)
  VEH_SUSPENSION_RF (186)
  VEH_SUSPENSION_LM (14)
  VEH_SUSPENSION_RM (16)
  VEH_SUSPENSION_LR (181)
  VEH_SUSPENSION_RR (180)
  VEH_TRANSMISSION_F (54)
  VEH_TRANSMISSION_M (14)
  VEH_TRANSMISSION_R (58)
  VEH_WHEELHUB_LF (141)
  VEH_WHEELHUB_RF (140)
  VEH_WHEELHUB_LR (140)
  VEH_WHEELHUB_RR (140)
  VEH_WHEELHUB_LM1 (2)
  VEH_WHEELHUB_RM1 (2)
  VEH_WHEELHUB_LM2 (2)
  VEH_WHEELHUB_RM2 (2)
  VEH_WHEELHUB_LM3 (2)
  VEH_WHEELHUB_RM3 (2)
  VEH_WINDSCREEN (193)
  VEH_WINDSCREEN_R (141)
  VEH_WINDOW_LF (205)
  VEH_WINDOW_RF (201)
  VEH_WINDOW_LR (153)
  VEH_WINDOW_RR (151)
  VEH_WINDOW_LM (27)
  VEH_WINDOW_RM (27)
  VEH_BODYSHELL (226)
  VEH_BUMPER_F (171)
  VEH_BUMPER_R (142)
  VEH_WING_RF (118)
  VEH_WING_LF (118)
  VEH_BONNET (168)
  VEH_BOOT (136)
  VEH_EXHAUST (208)
  VEH_EXHAUST_2 (136)
  VEH_EXHAUST_3 (12)
  VEH_EXHAUST_4 (8)
  VEH_ENGINE (217)
  VEH_OVERHEAT (220)
  VEH_OVERHEAT_2 (210)
  VEH_PETROLCAP (6)
  VEH_PETROLTANK (36)
  VEH_PETROLTANK_L (2)
  VEH_PETROLTANK_R (2)
  VEH_STEERING_WHEEL (6)
  VEH_HBGRIP_L (6)
  VEH_HBGRIP_R (6)
  VEH_HEADLIGHT_L (198)
  VEH_HEADLIGHT_R (198)
  VEH_TAILLIGHT_L (184)
  VEH_TAILLIGHT_R (182)
  VEH_INDICATOR_LF (143)
  VEH_INDICATOR_RF (143)
  VEH_INDICATOR_LR (184)
  VEH_INDICATOR_RR (183)
  VEH_BRAKELIGHT_L (184)
  VEH_BRAKELIGHT_R (182)
  VEH_BRAKELIGHT_M (16)
  VEH_REVERSINGLIGHT_L (141)
  VEH_REVERSINGLIGHT_R (137)
  VEH_EXTRALIGHT_1 (8)
  VEH_EXTRALIGHT_2 (7)
  VEH_EXTRALIGHT_3 (1)
  VEH_EXTRALIGHT_4 (1)
  VEH_INTERIORLIGHT (205)
  VEH_SIREN_1 (35)
  VEH_SIREN_2 (34)
  VEH_SIREN_3 (31)
  VEH_SIREN_4 (30)
  VEH_SIREN_5 (11)
  VEH_SIREN_6 (11)
  VEH_SIREN_7 (10)
  VEH_SIREN_8 (6)
  VEH_SIREN_9 (5)
  VEH_SIREN_10 (5)
  VEH_SIREN_11 (5)
  VEH_SIREN_12 (5)
  VEH_SIREN_13 (4)
  VEH_SIREN_14 (4)
  VEH_SIREN_15 (3)
  VEH_SIREN_16 (2)
  VEH_SIREN_17 (2)
  VEH_SIREN_18 (2)
  VEH_SIREN_19 (2)
  VEH_SIREN_20 (2)
  VEH_SIREN_GLASS_1 (10)
  VEH_SIREN_GLASS_2 (12)
  VEH_SIREN_GLASS_3 (9)
  VEH_SIREN_GLASS_4 (3)
  VEH_SIREN_GLASS_6 (2)
  VEH_SIREN_GLASS_7 (1)
  VEH_SIREN_GLASS_8 (1)
  VEH_WEAPON_1A (7)
  VEH_WEAPON_1B (3)
  VEH_WEAPON_2A (4)
  VEH_WEAPON_2B (4)
  VEH_WEAPON_3A (1)
  VEH_TURRET_1_BASE (4)
  VEH_TURRET_1_BARREL (1)
  VEH_ATTACH (12)
  VEH_ROOF (1)
  VEH_SOFTTOP_1 (8)
  VEH_SOFTTOP_2 (8)
  VEH_SOFTTOP_3 (8)
  VEH_SOFTTOP_4 (7)
  VEH_SOFTTOP_5 (7)
  VEH_SOFTTOP_6 (7)
  VEH_SOFTTOP_7 (4)
  VEH_SOFTTOP_8 (3)
  VEH_SOFTTOP_9 (3)
  VEH_SOFTTOP_10 (2)
  VEH_SOFTTOP_11 (2)
  VEH_SOFTTOP_12 (2)
  VEH_SOFTTOP_13 (1)
  VEH_HANDLER_FRAME_1 (1)
  VEH_HANDLER_FRAME_2 (1)
  VEH_DOOR_HATCH_R (1)
  VEH_DOOR_HATCH_L (1)
  VEH_TOW_ARM (2)
  VEH_TOW_MOUNT_A (2)
  VEH_TOW_MOUNT_B (2)
  VEH_ARM1 (1)
  VEH_ARM2 (1)
  VEH_DIGGER_ARM (1)
  VEH_ARTICULATED_DIGGER_ARM_BASE (226)
  VEH_ARTICULATED_DIGGER_ARM_BOOM (1)
  VEH_ARTICULATED_DIGGER_ARM_STICK (1)
  VEH_ARTICULATED_DIGGER_ARM_BUCKET (3)
  VEH_CUTTER_ARM_1 (1)
  VEH_CUTTER_ARM_2 (1)
  VEH_MISC_A (99)
  VEH_MISC_B (77)
  VEH_MISC_C (49)
  VEH_MISC_D (33)
  VEH_MISC_E (14)
  VEH_MISC_F (12)
  VEH_MISC_G (8)
  VEH_MISC_H (5)
  VEH_MISC_I (13)
  VEH_MISC_J (4)
  VEH_MISC_K (11)
  VEH_MISC_L (8)
  VEH_MISC_M (10)
  VEH_MISC_N (10)
  VEH_MISC_O (10)
  VEH_MISC_P (10)
  VEH_MISC_Q (5)
  VEH_MISC_R (4)
  VEH_MISC_S (4)
  VEH_MISC_T (2)
  VEH_MISC_U (2)
  VEH_MISC_V (2)
  VEH_MISC_W (2)
  VEH_MISC_X (2)
  VEH_MISC_Y (2)
  VEH_MISC_Z (2)
  VEH_MISC_1 (2)
  VEH_MISC_2 (2)
  VEH_EXTRA_1 (81)
  VEH_EXTRA_2 (50)
  VEH_EXTRA_3 (24)
  VEH_EXTRA_4 (21)
  VEH_EXTRA_5 (26)
  VEH_EXTRA_6 (14)
  VEH_EXTRA_7 (12)
  VEH_EXTRA_8 (3)
  VEH_EXTRA_9 (1)
  VEH_EXTRA_10 (1)
  VEH_EXTRA_11 (1)
  VEH_EXTRA_12 (1)

vehicle hierarchy IDs which are not used:
  VEH_CHASSIS_DUMMY
  VEH_NUMBERPLATE
  VEH_SIREN_GLASS_5
  VEH_SIREN_GLASS_9
  VEH_SIREN_GLASS_10
  VEH_SIREN_GLASS_11
  VEH_SIREN_GLASS_12
  VEH_SIREN_GLASS_13
  VEH_SIREN_GLASS_14
  VEH_SIREN_GLASS_15
  VEH_SIREN_GLASS_16
  VEH_SIREN_GLASS_17
  VEH_SIREN_GLASS_18
  VEH_SIREN_GLASS_19
  VEH_SIREN_GLASS_20
  VEH_WEAPON_1C
  VEH_WEAPON_1D
  VEH_WEAPON_2C
  VEH_WEAPON_2D
  VEH_WEAPON_3B
  VEH_WEAPON_3C
  VEH_WEAPON_3D
  VEH_TURRET_2_BASE
  VEH_TURRET_2_BARREL
  VEH_SEARCHLIGHT_BASE
  VEH_SEARCHLIGHT_BARREL
  VEH_ROOF2
  VEH_SOFT1
  VEH_FORKS
  VEH_MAST
  VEH_CARRIAGE
  VEH_FORKS_ATTACH
  VEH_TIPPER
  VEH_PICKUP_REEL
  VEH_AUGER
  VEH_SLIPSTREAM_L
  VEH_SLIPSTREAM_R
  VEH_ARM3
  VEH_ARM4
  VEH_CUTTER_BOOM_DRIVER
  VEH_CUTTER_CUTTER_DRIVER
  VEH_DIGGER_SHOVEL_2
  VEH_DIGGER_SHOVEL_3
  VEH_EXTRA_10
  VEH_SPRING_RF
  VEH_SPRING_LF
  VEH_SPRING_RR
  VEH_SPRING_LR

vehicle hierarchy ID max bone index = 92
vehicle component ID max count = 54
vehicle texcoord range [0.000000,0.000000,0.000000,0.000000]..[0.999985,0.999985,0.999985,0.999985] (NOT VALID DUE TO COMPRESSION)
max triangle error P = 0.000000
max triangle error T = 0.000015
max triangle error N = 0.007812
max triangle error C = 0.003891
*/

static void GetVehicleComponentToHierarchyMapping(const CVehicle* pVehicle, int compToHierIds[128], bool bReportWarnings = false)
{
	int hierToCompIds[VEH_NUM_NODES];

	if (compToHierIds)
	{
		sysMemSet(compToHierIds, -1, 128*sizeof(int));
	}

	atMap<int,bool> compToHierIds_map[128];

	for (int hierId = 0; hierId < VEH_NUM_NODES; hierId++)
	{
		const int boneIndex = pVehicle->GetBoneIndex((eHierarchyId)hierId);

		if (boneIndex != -1)
		{
			const int compId = pVehicle->GetFragInst()->GetComponentFromBoneIndex(boneIndex); // NOTE -- this only gets the "root" component for this bone index

			if (1 || bReportWarnings)
			{
				const fragType* pFragType = pVehicle->GetFragInst()->GetType();
				atMap<int,bool> compIds_map;

				// check all the physics LODs
				for (int currentLOD = 0; const_cast<fragType*>(pFragType)->GetPhysicsLODGroup()->GetLODByIndex(currentLOD); currentLOD++)
				{
					Assert(pFragType->GetComponentFromBoneIndex((fragInst::ePhysicsLOD)currentLOD, boneIndex) == compId);

					fragPhysicsLOD* pFragPhysicsLOD = pFragType->GetPhysics(currentLOD);

					// check them all ..
					for (int j = 0; j < pFragPhysicsLOD->GetNumChildren(); j++)
					{
						if (pFragType->GetBoneIndexFromID(pFragPhysicsLOD->GetChild(j)->GetBoneID()) == boneIndex)
						{
							compIds_map[j] = true;
						}
					}
				}

				if (compIds_map.GetNumUsed() != 1 && (compIds_map.GetNumUsed() > 1 || compId != -1))
				{
					char compIdStr[256] = "";

					for (atMap<int,bool>::Iterator iter = compIds_map.CreateIterator(); iter; ++iter)
					{
						if (compIdStr[0] != '\0')
						{
							strcat(compIdStr, ",");
						}

						strcat(compIdStr, atVarString("%d", iter.GetKey()).c_str());
					}

					if (hierId != VEH_CHASSIS)
					{
						//Assertf(0, "%s: %s (boneIndex %d) maps to multiple components [%s]", pVehicle->GetModelName(), GetVehicleHierarchyIdName(hierId), boneIndex, compIdStr);
					}
				}
			}

			hierToCompIds[hierId] = compId;

			if (compId >= 0 && compId < 128)
			{
				if (bReportWarnings)
				{
					compToHierIds_map[compId][hierId] = true;
				}

				if (compToHierIds && compToHierIds[compId] == -1)
				{
					compToHierIds[compId] = hierId;
				}
			}
			else if (bReportWarnings)
			{
				//Displayf("  %s (boneIndex %d) does not have a componentId", GetVehicleHierarchyIdName(hierId), boneIndex);
			}
		}
		else if (bReportWarnings)
		{
			//Displayf("  %s does not have a boneIndex", GetVehicleHierarchyIdName(hierId));
		}
	}

	for (int compId = 0; compId < 128; compId++)
	{
		if (compToHierIds_map[compId].GetNumUsed() > 1)
		{
			char compToHierIdStr[256] = "";

			for (atMap<int,bool>::Iterator iter = compToHierIds_map[compId].CreateIterator(); iter; ++iter)
			{
				if (compToHierIdStr[0] != '\0')
				{
					strcat(compToHierIdStr, ",");
				}

				strcat(compToHierIdStr, GetVehicleHierarchyIdName(iter.GetKey()));
			}

			Displayf("  componentId %d maps to multiple hierarchyIds [%s]", compId, compToHierIdStr);
		}
	}
}

static const char* GetVehicleTypeStr(VehicleType type)
{
	static const char* vehicleTypeStrings[] =
	{
		 STRING(VEHICLE_TYPE_CAR         ),
		 STRING(VEHICLE_TYPE_PLANE       ),
		 STRING(VEHICLE_TYPE_TRAILER     ),
		 STRING(VEHICLE_TYPE_QUADBIKE    ),
		 STRING(VEHICLE_TYPE_DRAFT       ),
GTA_ONLY(STRING(VEHICLE_TYPE_SUBMARINECAR)),
GTA_ONLY(STRING(VEHICLE_TYPE_AMPHIBIOUS_AUTOMOBILE)),
GTA_ONLY(STRING(VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE)),
		 STRING(VEHICLE_TYPE_HELI        ),
		 STRING(VEHICLE_TYPE_BLIMP       ),
RDR_ONLY(STRING(VEHICLE_TYPE_BALLOON     )),
		 STRING(VEHICLE_TYPE_AUTOGYRO    ),
		 STRING(VEHICLE_TYPE_BIKE        ),
		 STRING(VEHICLE_TYPE_BICYCLE     ),
		 STRING(VEHICLE_TYPE_BOAT        ),
RDR_ONLY(STRING(VEHICLE_TYPE_CANOE       )),
		 STRING(VEHICLE_TYPE_TRAIN       ),
		 STRING(VEHICLE_TYPE_SUBMARINE   ),
	};
	CompileTimeAssert(NELEM(vehicleTypeStrings) == VEHICLE_TYPE_NUM);

	return vehicleTypeStrings[type];
}

static atString GetVehicleHierarchyIdFullName(const CVehicle* pVehicle, const int compToHierIds[128], int componentId, int* hierarchyId_out = NULL)
{
	const char* hierarchyIdName = "$UNKNOWN";
	int         hierarchyId = -1;

	if (AssertVerify(componentId >= 0 && componentId < 128))
	{
		hierarchyId     = compToHierIds[componentId];
		hierarchyIdName = GetVehicleHierarchyIdName(hierarchyId);
	}

	if (hierarchyId_out)
	{
		*hierarchyId_out = hierarchyId;
	}

	if (hierarchyIdName[0] == '$')
	{
		Drawable* pDrawable = pVehicle->GetDrawable();
		crSkeletonData* pSkeletonData = pDrawable ? pDrawable->GetSkeletonData() : NULL;

		char boneNames[1024] = ""; // things like buses may have a lot of window bones
		int numBonesInComponent = 0;

		if (hierarchyId != -1)
		{
			if (pSkeletonData)
			{
				const int boneIndex = pVehicle->GetBoneIndex((eHierarchyId)hierarchyId);

				if (AssertVerify(boneIndex >= 0 && boneIndex < pSkeletonData->GetNumBones()))
				{
					strcpy(boneNames, pSkeletonData->GetBoneData(boneIndex)->GetName());
					numBonesInComponent++;
				}
			}
		}

		if (numBonesInComponent == 0) // get bone name directly from componentId
		{
			if (pSkeletonData)
			{
				for (int boneIndex = 0; boneIndex < pSkeletonData->GetNumBones(); boneIndex++)
				{
					if (pVehicle->GetFragInst()->GetComponentFromBoneIndex(boneIndex) == componentId) // NOTE -- this only gets the "root" component for this bone index
					{
						if (++numBonesInComponent > 1)
						{
							Assertf(0, "%s: has multiple bones in componentId=%d", pVehicle->GetModelName(), componentId);
							strcat(boneNames, ",");
						}

						if (AssertVerify(pSkeletonData->GetBoneData(boneIndex)))
						{
							strcat(boneNames, pSkeletonData->GetBoneData(boneIndex)->GetName());
						}
					}
				}
			}
			else if (pDrawable)
			{
				strcpy(boneNames, "NO SKELETON DATA??");
			}
			else
			{
				strcpy(boneNames, "NO DRAWABLE??");
			}
		}

		return atVarString("COMPONENT_%d(%s)", componentId, boneNames);
	}

	return atString(hierarchyIdName);
}

static u32             g_vehglassSmashTestFirstModelIndex = ~0UL;
static int             g_vehglassHierarchyIdCounts[VEH_NUM_NODES] = {-1};
static atMap<u32,bool> g_vehglassHierarchyIdModels;
static int             g_vehglassHierarchyIdMaxBoneIndex = 0;
static int             g_vehglassComponentIdMaxCount = 0;
static Vec4V           g_vehglassTexcoordMin = +Vec4V(V_FLT_MAX);
static Vec4V           g_vehglassTexcoordMax = -Vec4V(V_FLT_MAX);
static ScalarV         g_vehglassMaxTriangleErrorP = ScalarV(V_ZERO);
static ScalarV         g_vehglassMaxTriangleErrorT = ScalarV(V_ZERO);
static ScalarV         g_vehglassMaxTriangleErrorN = ScalarV(V_ZERO);
static ScalarV         g_vehglassMaxTriangleErrorC = ScalarV(V_ZERO);

float FindMostAcuteAngleInBoundGeometry(const char* name, phBoundGeometry* pGeom) // TODO -- put this in the smash test (see BS#543507)
{
	ScalarV d(V_NEGONE);

	for (int i = 0; i < pGeom->GetNumPolygons(); i++)
	{
		for (int j = i + 1; j < pGeom->GetNumPolygons(); j++)
		{
			const phPolygon& i_triangle = pGeom->GetPolygon(i);
			const phPolygon& j_triangle = pGeom->GetPolygon(j);

			for (int i_side = 0; i_side < POLY_MAX_VERTICES; i_side++)
			{
				phPolygon::Index i_index0 = i_triangle.GetVertexIndex((i_side + 0));
				phPolygon::Index i_index1 = i_triangle.GetVertexIndex((i_side + 1)%3);

				const Vec3V i_p0 = pGeom->GetVertex(i_index0);
				const Vec3V i_p1 = pGeom->GetVertex(i_index1);

				for (int j_side = 0; j_side < POLY_MAX_VERTICES; j_side++)
				{
					phPolygon::Index j_index0 = j_triangle.GetVertexIndex((j_side + 0));
					phPolygon::Index j_index1 = j_triangle.GetVertexIndex((j_side + 1)%3);

					const Vec3V j_p0 = pGeom->GetVertex(j_index0);
					const Vec3V j_p1 = pGeom->GetVertex(j_index1);

					bool bAdjacent = false;

					if (i_index0 == j_index1 &&
						i_index1 == j_index0)
					{
						bAdjacent = true;
					}
					else if (IsEqualAll(i_p0, j_p1) & IsEqualAll(i_p1, j_p0))
					{
						Assertf(0, "### %s: triangle %d and triangle %d are physically adjacent but don't share indices", name, i, j);
						Displayf(  "### %s: triangle %d and triangle %d are physically adjacent but don't share indices", name, i, j);
						bAdjacent = true;
					}

					if (bAdjacent)
					{
						const Vec3V i_p2 = pGeom->GetVertex(i_triangle.GetVertexIndex((i_side + 2)%3));
						const Vec3V j_p2 = pGeom->GetVertex(j_triangle.GetVertexIndex((j_side + 2)%3));

						const Vec3V i_n = Normalize(Cross(i_p1 - i_p0, i_p2 - i_p0));
						const Vec3V j_n = Normalize(Cross(j_p1 - j_p0, j_p2 - j_p0));

						d = Max(Dot(i_n, j_n), d);
					}
				}
			}
		}
	}

	return acosf(d.Getf())*RtoD;
}

#if VEHICLE_GLASS_CONSTRUCTOR

void DumpVehicleWindowMaterialsCSV(const CVehicle* pVehicle, const atArray<fwVehicleGlassWindowProxy*>& proxies)
{
	static fiStream* fp = NULL;

	if (fp == NULL)
	{
		fp = fiStream::Create("assets:/non_final/vgwindows/vehicle_window_materials.csv");

		if (fp)
		{
			fprintf(fp, "name,");

			for (int i = VEH_FIRST_WINDOW; i <= VEH_LAST_WINDOW; i++)
			{
				fprintf(fp, "%s,", GetVehicleHierarchyIdName(i) + strlen("VEH_"));
			}

			fprintf(fp, "\n");
		}
	}

	if (fp)
	{
		const phArchetype* pArchetype = pVehicle->GetFragInst()->GetArchetype();

		if (AssertVerify(pArchetype))
		{
			const phBound* pBound = pArchetype->GetBound();

			if (AssertVerify(pBound->GetType() == phBound::COMPOSITE))
			{
				class GetBoundChildMaterialName { public: static void func(char mtlIdName[128], int nameLen, const phBound* pBoundChild)
				{
					if (pBoundChild)
					{
						if (pBoundChild->GetNumMaterials() > 0)
						{
							if (pBoundChild->GetType() == phBound::GEOMETRY ||
								pBoundChild->GetType() == phBound::BVH)
							{
								const phMaterialMgr::Id mtlId = PGTAMATERIALMGR->UnpackMtlId(pBoundChild->GetMaterialId(0));
								PGTAMATERIALMGR->GetMaterialName(mtlId, mtlIdName, nameLen);

								const char* prefix = "car_glass_";

								if (memcmp(mtlIdName, prefix, strlen(prefix)) == 0) // remove prefix
								{
									strcpy(mtlIdName, atVarString("%s", mtlIdName).c_str() + strlen(prefix));
								}

								if (strrchr(mtlIdName, '_')) // strip trailing material index
								{
									strrchr(mtlIdName, '_')[0] = '\0';
								}
							}

							if (pBoundChild->GetNumMaterials() > 1)
							{
								strcat(mtlIdName, atVarString("[%d]", pBoundChild->GetNumMaterials()).c_str());
							}
						}
						else
						{
							strcpy(mtlIdName, "[no materials]");
						}
					}
					else
					{
						strcpy(mtlIdName, "[no bound]");
					}
				}};

				const phBoundComposite* pBoundComposite = static_cast<const phBoundComposite*>(pBound);
				u64 used = 0;

				fprintf(fp, "%s,", pVehicle->GetModelName());

				for (int i = VEH_FIRST_WINDOW; i <= VEH_LAST_WINDOW; i++)
				{
					bool bFound = false;

					for (int j = 0; j < proxies.GetCount(); j++)
					{
						if (pVehicle->GetBoneIndex((eHierarchyId)i) == proxies[j]->m_boneIndex)
						{
							char mtlIdName[128] = "???";
							GetBoundChildMaterialName::func(mtlIdName, sizeof(mtlIdName), pBoundComposite->GetBound(proxies[j]->m_componentId));

							fprintf(fp, "%s,", mtlIdName);
							bFound = true;
							Assert(j < 64);
							used |= BIT64(j);
							break;
						}
					}

					if (!bFound)
					{
						fprintf(fp, "-,");
					}
				}

				for (int j = 0; j < proxies.GetCount(); j++)
				{
					if ((used & BIT64(j)) == 0)
					{
						char mtlIdName[128] = "???";
						GetBoundChildMaterialName::func(mtlIdName, sizeof(mtlIdName), pBoundComposite->GetBound(proxies[j]->m_componentId));

						fprintf(fp, "%s=%s,", pVehicle->GetDrawable()->GetSkeletonData()->GetBoneData(proxies[j]->m_boneIndex)->GetName(), mtlIdName);
					}
				}

				fprintf(fp, "\n");
				fp->Flush();
			}
		}
	}
}

#endif // VEHICLE_GLASS_CONSTRUCTOR

void CVehicleGlassSmashTestManager::SmashTest(CVehicle* pVehicle)
{
	if (pVehicle && m_smashTest)
	{
		Channel_art      .TtyLevel = DIAG_SEVERITY_FATAL;
		Channel_Defrag   .TtyLevel = DIAG_SEVERITY_FATAL;
		Channel_modelinfo.TtyLevel = DIAG_SEVERITY_FATAL;
		Channel_vfx      .TtyLevel = DIAG_SEVERITY_FATAL;
		Channel_vfx_ptfx .TtyLevel = DIAG_SEVERITY_FATAL;

		if (0) // check window data for BS#1007119
		{
			const bool bSuperVerbose = true;

			const fragInst* pFragInst = pVehicle->GetFragInst();
			const fwVehicleGlassWindowData* pWindowData = static_cast<const gtaFragType*>(pFragInst->GetType())->m_vehicleWindowData;

			if (pWindowData)
			{
				const phBound* pBound = pFragInst->GetArchetype()->GetBound();
				const int componentCount = (pBound->GetType() == phBound::COMPOSITE) ? static_cast<const phBoundComposite*>(pBound)->GetNumBounds() : 1;

				for (int componentId = 0; componentId < componentCount; componentId++)
				{
					const fwVehicleGlassWindow* pWindow = fwVehicleGlassWindowData::GetWindow(pWindowData, componentId);

					if (pWindow)
					{
						const fragTypeChild* pFragChild = pFragInst->GetTypePhysics()->GetChild(componentId);
						const int boneIndex = pVehicle->GetFragInst()->GetType()->GetBoneIndexFromID(pFragChild->GetBoneID());
						const char* boneName = pVehicle->GetDrawable()->GetSkeletonData()->GetBoneData(boneIndex)->GetName();

						Assertf(pWindow->m_tag == 'VGWC', "%s: tag is 0x%08x", pVehicle->GetModelName(), pWindow->m_tag);

						const u16* rowOffsets = (const u16*)(pWindow->m_dataRLE);

						if (rowOffsets)
						{
							const int offset = (int)(size_t)rowOffsets - (int)(size_t)pWindow;

							if (bSuperVerbose)
							{
								Displayf("%s: offset is %d for '%s'", pVehicle->GetModelName(), offset, boneName);
							}

							Assertf(offset >= sizeof(fwVehicleGlassWindowBase), "%s: offset is %d for '%s', m_dataRLE = 0x%p, pWindow = 0x%p", pVehicle->GetModelName(), offset, boneName, pWindow->m_dataRLE, pWindow);
							Assertf(offset <= sizeof(fwVehicleGlassWindow),     "%s: offset is %d for '%s', m_dataRLE = 0x%p, pWindow = 0x%p", pVehicle->GetModelName(), offset, boneName, pWindow->m_dataRLE, pWindow);

							if (bSuperVerbose)
							{
								for (int j = 0; j < pWindow->m_dataRows; j++)
								{
									const u8* data = pWindow->m_dataRLE + pWindow->m_dataRows*sizeof(u16) + (int)rowOffsets[j];
									const int dataOffset = (int)(size_t)data - (int)(size_t)pWindow;

									Displayf("%s: data for row %d is 0x%p (offset = %d) for '%s'", pVehicle->GetModelName(), j, data, dataOffset, boneName);
								}
							}
						}
						else if (bSuperVerbose)
						{
							Displayf("%s: no RLE data for '%s'", pVehicle->GetModelName(), boneName);
						}
					}
				}
			}
		}

#if __CONSOLE
		if (stricmp(pVehicle->GetModelName(), "entityxfpchi") == 0)
		{
			return; // skip smash test for this vehicle, it's a pc test vehicle with extremely high tessellation
		}
#endif // __CONSOLE

		// check boneIndex/componentId mapping (BS#829377)
		{
			const bool bReportOkBones = false;

			fragInst* pFragInst = pVehicle->GetFragInst();

			if (AssertVerify(pFragInst && pFragInst == pVehicle->GetFragInst()))
			{
				const phBound* pBound = pFragInst->GetArchetype()->GetBound();

				if (AssertVerify(pBound && pBound->GetType() == phBound::COMPOSITE))
				{
					const phBoundComposite* pBoundComposite = static_cast<const phBoundComposite*>(pBound);

					for (int boneIndex = 0; boneIndex < pVehicle->GetDrawable()->GetSkeletonData()->GetNumBones(); boneIndex++)
					{
						const char* boneName = pVehicle->GetDrawable()->GetSkeletonData()->GetBoneData(boneIndex)->GetName();
						const int boneGroupId = pFragInst->GetGroupFromBoneIndex(boneIndex);

						if (boneGroupId != -1)
						{
							const fragTypeGroup* pGroup = pFragInst->GetTypePhysics()->GetGroup(boneGroupId);

							if (pGroup->GetNumChildren() > 0)
							{
								const int firstComponentId = pGroup->GetChildFragmentIndex();
								const int lastComponentId = pGroup->GetChildFragmentIndex() + pGroup->GetNumChildren() - 1;
								const int rootComponentId = pFragInst->GetComponentFromBoneIndex(boneIndex);

								if (rootComponentId < pGroup->GetChildFragmentIndex() ||
									rootComponentId > pGroup->GetChildFragmentIndex() + pGroup->GetNumChildren() - 1)
								{
									Displayf(
										"### %s: bone '%s' (index=%d, group=%d) root component is %d, expected [%d..%d]",
										pVehicle->GetModelName(),
										boneName,
										boneIndex,
										boneGroupId,
										rootComponentId,
										firstComponentId,
										lastComponentId
									);
								}

								for (int k = 0; k < pGroup->GetNumChildren(); k++)
								{
									const int componentId = pGroup->GetChildFragmentIndex() + k;

									Assert(componentId < pBoundComposite->GetNumBounds());

									const fragTypeChild* pFragChild = pFragInst->GetTypePhysics()->GetChild(componentId);
									const int boneIndex2 = pFragInst->GetType()->GetBoneIndexFromID(pFragChild->GetBoneID());
									const char* boneName2 = pVehicle->GetDrawable()->GetSkeletonData()->GetBoneData(boneIndex2)->GetName();

									if (boneIndex != boneIndex2)
									{
										Displayf(
											"### %s: bone '%s' (index=%d, group=%d) has component %d [%d..%d] which is attached to bone '%s' (index=%d)",
											pVehicle->GetModelName(),
											boneName,
											boneIndex,
											boneGroupId,
											componentId,
											firstComponentId,
											lastComponentId,
											boneName2,
											boneIndex2
										);
									}
									else if (bReportOkBones)
									{
										Displayf(
											"%s: bone '%s' (index=%d, group=%d) has component %d [%d..%d] which is ok",
											pVehicle->GetModelName(),
											boneName,
											boneIndex,
											boneGroupId,
											componentId,
											firstComponentId,
											lastComponentId
										);
									}
								}
							}
							else
							{
								Displayf(
									"### %s: bone '%s' (index=%d, group=%d) has no components",
									pVehicle->GetModelName(),
									boneName,
									boneIndex,
									boneGroupId
								);
							}
						}
						else // not a group .. then we shouldn't have any components
						{
							const int rootComponentId = pFragInst->GetComponentFromBoneIndex(boneIndex);
							bool bOk = true;

							if (rootComponentId != -1)
							{
								Displayf(
									"### %s: bone '%s' (index=%d) has no group but has a root component %d, expected -1",
									pVehicle->GetModelName(),
									boneName,
									boneIndex,
									rootComponentId
								);

								bOk = false;
							}

							for (int componentId = 0; componentId < pBoundComposite->GetNumBounds(); componentId++)
							{
								const fragTypeChild* pFragChild = pFragInst->GetTypePhysics()->GetChild(componentId);
								const int boneIndex2 = pFragInst->GetType()->GetBoneIndexFromID(pFragChild->GetBoneID());

								if (boneIndex2 == boneIndex)
								{
									Displayf(
										"### %s: bone '%s' (index=%d) has no group component %d maps to it",
										pVehicle->GetModelName(),
										boneName,
										boneIndex,
										componentId
									);

									bOk = false;
								}
							}

							if (bOk && bReportOkBones)
							{
								Displayf(
									"%s: bone '%s' (index=%d) has no group but appears to be ok",
									pVehicle->GetModelName(),
									boneName,
									boneIndex
								);
							}
						}
					}
				}
			}
		}

		const char* vehicleTypeStr = GetVehicleTypeStr(pVehicle->GetVehicleType());
		char vehicleModelDesc[1024] = "";
		sprintf(vehicleModelDesc, "%s (%s) ", pVehicle->GetModelName(), vehicleTypeStr);

		while (strlen(vehicleModelDesc) < 40)
		{
			strcat(vehicleModelDesc, ".");
		}

		if (g_vehglassSmashTestFirstModelIndex == ~0UL)
		{
			g_vehglassSmashTestFirstModelIndex = pVehicle->GetModelIndex();
		}
		else if (g_vehglassSmashTestFirstModelIndex == pVehicle->GetModelIndex() && m_smashTestAuto)
		{
			if (m_smashTestSimple || m_smashTestConstructWindows)
			{
				// don't report
			}
			else
			{
				Displayf("");
				Displayf("vehicle hierarchy IDs which are used:");

				for (int j = 0; j < VEH_NUM_NODES; j++)
				{
					if (g_vehglassHierarchyIdCounts[j] > 0)
					{
						Displayf("  %s (%d)", GetVehicleHierarchyIdName(j), g_vehglassHierarchyIdCounts[j]);
					}
				}

				Displayf("");
				Displayf("vehicle hierarchy IDs which are not used:");

				for (int j = 0; j < VEH_NUM_NODES; j++)
				{
					if (g_vehglassHierarchyIdCounts[j] == 0)
					{
						Displayf("  %s", GetVehicleHierarchyIdName(j));
					}
				}

				Displayf("");
				Displayf("vehicle hierarchy ID max bone index = %d", g_vehglassHierarchyIdMaxBoneIndex);
				Displayf("vehicle component ID max count = %d", g_vehglassComponentIdMaxCount);
				Displayf(
					"vehicle texcoord range [%f,%f,%f,%f]..[%f,%f,%f,%f]%s",
					VEC4V_ARGS(g_vehglassTexcoordMin),
					VEC4V_ARGS(g_vehglassTexcoordMax),
					VEHICLE_GLASS_COMPRESSED_VERTICES ? " (NOT VALID DUE TO COMPRESSION)" : ""
				);
				Displayf("max triangle error P = %f", g_vehglassMaxTriangleErrorP.Getf());
				Displayf("max triangle error T = %f", g_vehglassMaxTriangleErrorT.Getf());
				Displayf("max triangle error N = %f", g_vehglassMaxTriangleErrorN.Getf());
				Displayf("max triangle error C = %f", g_vehglassMaxTriangleErrorC.Getf());
			}

			m_smashTest = false;
			m_smashTestAuto = false;
			return;
		}

		if (m_smashTestSimple)
		{
			return;
		}
		else if (m_smashTestOutputWindowFlagsOnly)
		{
			const fwVehicleGlassWindowData* pWindowData = static_cast<const gtaFragType*>(pVehicle->GetFragInst()->GetType())->m_vehicleWindowData;
			static fiStream* csv = NULL;
			
			if (csv == NULL)
			{
				csv = fiStream::Create("assets:/non_final/vgwindows/vehicle_window_flags.csv");
			}

			if (pWindowData)
			{
				const phBound* pBound = pVehicle->GetFragInst()->GetArchetype()->GetBound();
				const int componentCount = (pBound->GetType() == phBound::COMPOSITE) ? static_cast<const phBoundComposite*>(pBound)->GetNumBounds() : 1;

				for (int componentId = 0; componentId < componentCount; componentId++)
				{
					atArray<phMaterialMgr::Id> mtlIds;
					bool materialIsSmashableGlass = false;

					if (GetMaterialIdsFromComponentId(mtlIds, pVehicle, componentId))
					{
						for (int i = 0; i < mtlIds.GetCount(); i++)
						{
							const phMaterialMgr::Id materialId = PGTAMATERIALMGR->UnpackMtlId(mtlIds[i]);

							if (PGTAMATERIALMGR->GetIsSmashableGlass(materialId))
							{
								materialIsSmashableGlass = true;
								break;
							}
						}
					}

					if (materialIsSmashableGlass)
					{
						const fragTypeChild* pFragChild = pVehicle->GetFragInst()->GetTypePhysics()->GetChild(componentId);
						const int boneIndex = pVehicle->GetFragInst()->GetType()->GetBoneIndexFromID(pFragChild->GetBoneID());
						const char* boneName = pVehicle->GetDrawable()->GetSkeletonData()->GetBoneData(boneIndex)->GetName();

						const fwVehicleGlassWindow* pWindow = fwVehicleGlassWindowData::GetWindow(pWindowData, componentId);

						if (pWindow)
						{
							fprintf(csv, "%s,%s,0x%08x\n", pVehicle->GetModelName(), boneName, pWindow->GetFlags());
							csv->Flush();
						}
						else
						{
							fprintf(csv, "%s,%s,NULL WINDOW\n", pVehicle->GetModelName(), boneName);
							csv->Flush();
						}
					}
				}
			}
			else
			{
				fprintf(csv, "%s,,NULL WINDOW DATA\n", pVehicle->GetModelName());
				csv->Flush();
			}

			return;
		}
		else if (m_smashTestOutputOBJsOnly)
		{
			class g { public: static bool func(const grmModel&, const grmGeometry&, const grmShader& shader, void*)
			{
				return strcmp(shader.GetName(), "vehicle_vehglass") == 0;
			}};

			if (GeometryUtil::CountTrianglesForEntity(pVehicle, LOD_HIGH, g::func) > 0)
			{
				CDumpGeometryToOBJ dump(atVarString("assets:/non_final/vgwindows/%s.obj", pVehicle->GetModelName()).c_str());
				static CDumpGeometryToOBJ& _this = dump;

				class t { public: static void func(Vec3V_In v0, Vec3V_In v1, Vec3V_In v2, int, int, int, void*)
				{
					_this.AddTriangle(v0, v1, v2);
				}};

				GeometryUtil::AddTrianglesForEntity(pVehicle, LOD_HIGH, g::func, t::func, false);
				dump.Close();
			}

			return;
		}
		else if (m_smashTestConstructWindows)
		{
#if VEHICLE_GLASS_CONSTRUCTOR
			atArray<fwVehicleGlassWindowProxy*> proxies;

			fwVehicleGlassConstructorInterface::ConstructWindows(
				&proxies,
				pVehicle->GetModelName(),
				pVehicle->GetFragInst()->GetType(),
				NULL,
				0,
				g_vehicleGlassMan.m_distanceFieldOutputFiles,
				false,
				true,
				0
			);

			DumpVehicleWindowMaterialsCSV(pVehicle, proxies);

			fwVehicleGlassWindowData* data = fwVehicleGlassWindowData::ConstructWindowData(proxies.GetElements(), proxies.GetCount());

			if (data)
			{
				delete[] (u8*)data; // it's really an array of u8's
			}

			for (int i = 0; i < proxies.GetCount(); i++)
			{
				delete proxies[i];
			}
#endif // VEHICLE_GLASS_CONSTRUCTOR

			return;
		}

		//if (m_smashTest && bOkForSmashTest)
		{
			Assertf(!pVehicle->AreAnyBrokenFlagsSet(), "smash testing broken vehicle %s", pVehicle->GetModelName());

			const phBound* pBound = pVehicle->GetFragInst()->GetArchetype()->GetBound();
			const int componentCount = (pBound->GetType() == phBound::COMPOSITE) ? static_cast<const phBoundComposite*>(pBound)->GetNumBounds() : 1;

			VfxCollInfo_s info;

			info.regdEnt       = pVehicle;
			info.vPositionWld  = Vec3V(V_FLT_MAX);
			info.vNormalWld    = Vec3V(V_Z_AXIS_WZERO);
			info.vDirectionWld = Vec3V(V_Z_AXIS_WZERO);
			info.materialId    = PGTAMATERIALMGR->g_idCarGlassWeak;
			info.componentId   = -1;
			info.weaponGroup   = WEAPON_EFFECT_GROUP_PUNCH_KICK; // TODO: pass is correct weapon group?
			info.force         = 1.0f;
			info.isBloody      = false;
			info.isWater       = false;
			info.isExitFx      = false;
			info.noPtFx		   = false;
			info.noPedDamage   = false;
			info.noDecal	   = false; 
			info.isSnowball	   = false;
			info.isFMJAmmo	   = false;

			int numComponentsSmashable = 0;

			for (int componentId = 0; componentId < componentCount; componentId++)
			{
				const phMaterialMgr::Id mtlId = GetSmashableGlassMaterialId(pVehicle, componentId);

				if (mtlId != phMaterialMgr::MATERIAL_NOT_FOUND)
				{
					info.componentId = componentId;

					g_vehicleGlassMan.StoreCollision(info, false, (int)pVehicle->GetModelIndex());
					numComponentsSmashable++;
				}
			}

			if (Verifyf(pVehicle->GetFragInst(), "%s: has no fraginst", pVehicle->GetModelName()) &&
				Verifyf(pVehicle->GetFragInst()->GetTypePhysics(), "%s: has no fraginst physics", pVehicle->GetModelName()))
			{
				g_vehglassComponentIdMaxCount = Max<int>(pVehicle->GetFragInst()->GetTypePhysics()->GetNumChildren(), g_vehglassComponentIdMaxCount);
			}

			if (numComponentsSmashable == 0 && m_smashTestReportOk)
			{
				if (m_smashTestCompactOutput)
				{
					Displayf("%s ok, no smashable components", vehicleModelDesc);
				}
			}

			m_smashTestVehicle = pVehicle;
			m_smashTestVehicleMatrix = pVehicle->GetMatrix();
		}
	}
}

void CVehicleGlassSmashTestManager::SmashTestUpdate()
{
	CVehicle* pVehicle = m_smashTestVehicle.Get();

	if (pVehicle)
	{
		Mat34V matrix = m_smashTestVehicleMatrix;
		matrix.GetCol3Ref() = pVehicle->GetMatrix().GetCol3();

		pVehicle->SetMatrix(RCC_MATRIX34(matrix));

		const float dt = ((float)fwTimer::GetSystemTimeStepInMilliseconds())/1000.f;
		const ScalarV theta = ScalarV(dt*m_smashTestAutoRotation/m_smashTestAutoTimeStep*DtoR);

		m_smashTestVehicleMatrix.GetCol0Ref() = RotateAboutZAxis(m_smashTestVehicleMatrix.GetCol0(), theta);
		m_smashTestVehicleMatrix.GetCol1Ref() = RotateAboutZAxis(m_smashTestVehicleMatrix.GetCol1(), theta);
		m_smashTestVehicleMatrix.GetCol2Ref() = RotateAboutZAxis(m_smashTestVehicleMatrix.GetCol2(), theta);
	}
}

__COMMENT(static) void CVehicleGlassSmashTestManager::SmashTestAddTriangle(
	CVehicleGlassComponent* pComponent,
	const CVehicleGlassTriangle* pTriangle,
	Vec3V_In P0, Vec3V_In P1, Vec3V_In P2,
	Vec4V_In T0, Vec4V_In T1, Vec4V_In T2,
	Vec3V_In N0, Vec3V_In N1, Vec3V_In N2,
	Vec4V_In C0, Vec4V_In C1, Vec4V_In C2
	)
{
	if (g_vehicleGlassSmashTest.m_smashTest)
	{
		Vec3V P0_test, P1_test, P2_test;
		Vec4V T0_test, T1_test, T2_test;
		Vec3V N0_test, N1_test, N2_test;
		Vec4V C0_test, C1_test, C2_test;

		P0_test = pTriangle->GetVertexP(0);
		P1_test = pTriangle->GetVertexP(1);
		P2_test = pTriangle->GetVertexP(2);

		pTriangle->GetVertexTNC(0, T0_test, N0_test, C0_test);
		pTriangle->GetVertexTNC(1, T1_test, N1_test, C1_test);
		pTriangle->GetVertexTNC(2, T2_test, N2_test, C2_test);

		pComponent->m_smashTestMaxTriangleErrorP = Max(Abs(P0 - P0_test), pComponent->m_smashTestMaxTriangleErrorP);
		pComponent->m_smashTestMaxTriangleErrorP = Max(Abs(P1 - P1_test), pComponent->m_smashTestMaxTriangleErrorP);
		pComponent->m_smashTestMaxTriangleErrorP = Max(Abs(P2 - P2_test), pComponent->m_smashTestMaxTriangleErrorP);

		// need to clamp texcoords, as we might be compressing bogus values outside [0..1] range which would give misleading errors
		pComponent->m_smashTestMaxTriangleErrorT = Max(Abs(Clamp(T0, Vec4V(V_ZERO), Vec4V(V_ONE)) - Clamp(T0_test, Vec4V(V_ZERO), Vec4V(V_ONE))), pComponent->m_smashTestMaxTriangleErrorT);
		pComponent->m_smashTestMaxTriangleErrorT = Max(Abs(Clamp(T1, Vec4V(V_ZERO), Vec4V(V_ONE)) - Clamp(T1_test, Vec4V(V_ZERO), Vec4V(V_ONE))), pComponent->m_smashTestMaxTriangleErrorT);
		pComponent->m_smashTestMaxTriangleErrorT = Max(Abs(Clamp(T2, Vec4V(V_ZERO), Vec4V(V_ONE)) - Clamp(T2_test, Vec4V(V_ZERO), Vec4V(V_ONE))), pComponent->m_smashTestMaxTriangleErrorT);

		pComponent->m_smashTestMaxTriangleErrorN = Max(Abs(N0 - N0_test), pComponent->m_smashTestMaxTriangleErrorN);
		pComponent->m_smashTestMaxTriangleErrorN = Max(Abs(N1 - N1_test), pComponent->m_smashTestMaxTriangleErrorN);
		pComponent->m_smashTestMaxTriangleErrorN = Max(Abs(N2 - N2_test), pComponent->m_smashTestMaxTriangleErrorN);

		pComponent->m_smashTestMaxTriangleErrorC = Max(Abs(C0 - C0_test), pComponent->m_smashTestMaxTriangleErrorC);
		pComponent->m_smashTestMaxTriangleErrorC = Max(Abs(C1 - C1_test), pComponent->m_smashTestMaxTriangleErrorC);
		pComponent->m_smashTestMaxTriangleErrorC = Max(Abs(C2 - C2_test), pComponent->m_smashTestMaxTriangleErrorC);

		if (MaxElement(pComponent->m_smashTestMaxTriangleErrorP).Getf() > 0.000001f)
		{
			static bool bOnce = true;
			if (bOnce) { bOnce = false; __debugbreak(); }
		}

		if (MaxElement(pComponent->m_smashTestMaxTriangleErrorT).Getf() > 0.000016f)
		{
			static bool bOnce = true;
			if (bOnce) { bOnce = false; __debugbreak(); }
		}

		if (MaxElement(pComponent->m_smashTestMaxTriangleErrorN).Getf() > 0.008f)
		{
			static bool bOnce = true;
			if (bOnce) { bOnce = false; __debugbreak(); }
		}

		if (MaxElement(pComponent->m_smashTestMaxTriangleErrorC).Getf() > 0.004f)
		{
			static bool bOnce = true;
			if (bOnce) { bOnce = false; __debugbreak(); }
		}

		g_vehglassMaxTriangleErrorP = Max(MaxElement(pComponent->m_smashTestMaxTriangleErrorP), g_vehglassMaxTriangleErrorP);
		g_vehglassMaxTriangleErrorT = Max(MaxElement(pComponent->m_smashTestMaxTriangleErrorT), g_vehglassMaxTriangleErrorT);
		g_vehglassMaxTriangleErrorN = Max(MaxElement(pComponent->m_smashTestMaxTriangleErrorN), g_vehglassMaxTriangleErrorN);
		g_vehglassMaxTriangleErrorC = Max(MaxElement(pComponent->m_smashTestMaxTriangleErrorC), g_vehglassMaxTriangleErrorC);
	}
}

CVehicleGlassSmashTestManager::CSmashTest::CSmashTest(int modelIndex, const VfxCollInfo_s& info)
{
	m_modelIndex = modelIndex;
	m_collisions.PushAndGrow(info);
}

__COMMENT(static) int CVehicleGlassSmashTestManager::CSmashTest::CheckVehicle(const CVehicle* pVehicle)
{
	const char* vehicleTypeStr = GetVehicleTypeStr(pVehicle->GetVehicleType());
	char vehicleModelDesc[1024] = "";
	sprintf(vehicleModelDesc, "%s (%s) ", pVehicle->GetModelName(), vehicleTypeStr);

	while (strlen(vehicleModelDesc) < 40)
	{
		strcat(vehicleModelDesc, ".");
	}

	bool bHasSmashableGlass = false;
	int numWarnings = 0;

	// check bounds validity
	{
		phArchetype* pArchetype = pVehicle->GetFragInst()->GetArchetype();

		if (AssertVerify(pArchetype))
		{
			phBound* pBound = pArchetype->GetBound();

			if (AssertVerify(pBound->GetType() == phBound::COMPOSITE))
			{
				const phBoundComposite* pBoundComposite = static_cast<const phBoundComposite*>(pBound);
				const int componentCount = pBoundComposite->GetNumBounds();

				Assert(componentCount == pVehicle->GetFragInst()->GetTypePhysics()->GetNumChildren());

				int compToHierIds[128];
				GetVehicleComponentToHierarchyMapping(pVehicle, compToHierIds);

				for (int componentId = 0; componentId < componentCount; componentId++)
				{
					phBound* pBoundChild = pBoundComposite->GetBound(componentId);

					if (pBoundChild)
					{
						const fragTypeChild* pFragChild = pVehicle->GetFragInst()->GetTypePhysics()->GetChild(componentId);
						const int boneIndex = pVehicle->GetFragInst()->GetType()->GetBoneIndexFromID(pFragChild->GetBoneID());
						const char* boneName = pVehicle->GetDrawable()->GetSkeletonData()->GetBoneData(boneIndex)->GetName();

						if (pBoundChild->GetType() == phBound::GEOMETRY ||
							pBoundChild->GetType() == phBound::BVH)
						{
							if (pBoundChild->GetNumMaterials() == 1)
							{
								const phMaterialMgr::Id mtlId = PGTAMATERIALMGR->UnpackMtlId(pBoundChild->GetMaterialId(0));

								if (PGTAMATERIALMGR->GetIsSmashableGlass(mtlId)) // VFXGROUP_CAR_GLASS
								{
									// yes, it's smashable glass
									bHasSmashableGlass = true;
								}
								else
								{
									// no, it's not
								}
							}
							else // make sure none are smashable glass
							{
								int unexpectedSmashableGlassMaterialCount = 0;

								for (int i = 0; i < pBoundChild->GetNumMaterials(); i++)
								{
									const phMaterialMgr::Id mtlId = PGTAMATERIALMGR->UnpackMtlId(pBoundChild->GetMaterialId(i));

									if (PGTAMATERIALMGR->GetIsSmashableGlass(mtlId))
									{
										unexpectedSmashableGlassMaterialCount++;
									}
								}

								if (unexpectedSmashableGlassMaterialCount > 0)
								{
									Displayf("%s unexpected smashable glass materials in child bounds ('%s', componentId=%d, type=GEOMETRY)", vehicleModelDesc, boneName, componentId);
									numWarnings++;

									for (int i = 0; i < pBoundChild->GetNumMaterials(); i++)
									{
										const phMaterialMgr::Id mtlId = PGTAMATERIALMGR->UnpackMtlId(pBoundChild->GetMaterialId(i));
										int errCh = ' ';

										if (PGTAMATERIALMGR->GetIsSmashableGlass(mtlId))
										{
											errCh = '!';
										}

										char mtlIdName[128] = "";
										PGTAMATERIALMGR->GetMaterialName(mtlId, mtlIdName, sizeof(mtlIdName));
										Displayf("%c   material %d (of %d) is %s", errCh, i + 1, pBoundChild->GetNumMaterials(), mtlIdName);
									}

									//bOkForSmashTest = false;
								}
							}

							if (bHasSmashableGlass && g_vehicleGlassSmashTest.m_smashTestAcuteAngleCheck)
							{
								if (pBoundChild->GetType() == phBound::GEOMETRY)
								{
									phBoundGeometry* pBoundGeometry = static_cast<phBoundGeometry*>(pBoundChild);
									const float angle = FindMostAcuteAngleInBoundGeometry(pVehicle->GetModelName(), pBoundGeometry);

									// this is going to spam like crazy until we resolve BS#543507
									if (angle > 70.0f)
									{
										Displayf("%s smashable glass geometry bound ('%s', componentId=%d) has acute angles up to %f degrees", vehicleModelDesc, boneName, componentId, angle);
									}
								}
							}
						}
						else
						{
							int unexpectedSmashableGlassMaterialCount = 0;

							for (int i = 0; i < pBoundChild->GetNumMaterials(); i++)
							{
								const phMaterialMgr::Id mtlId = PGTAMATERIALMGR->UnpackMtlId(pBoundChild->GetMaterialId(i));

								if (PGTAMATERIALMGR->GetIsSmashableGlass(mtlId))
								{
									unexpectedSmashableGlassMaterialCount++;
								}
							}

							if (unexpectedSmashableGlassMaterialCount > 0)
							{
								Displayf("%s unexpected smashable glass materials in child bounds ('%s', componentId=%d, type=%d)", vehicleModelDesc, boneName, componentId, pBoundChild->GetType());
								numWarnings++;

								for (int i = 0; i < pBoundChild->GetNumMaterials(); i++)
								{
									const phMaterialMgr::Id mtlId = PGTAMATERIALMGR->UnpackMtlId(pBoundChild->GetMaterialId(i));
									int errCh = ' ';

									if (PGTAMATERIALMGR->GetIsSmashableGlass(mtlId))
									{
										errCh = '!';
									}

									char mtlIdName[128] = "";
									PGTAMATERIALMGR->GetMaterialName(mtlId, mtlIdName, sizeof(mtlIdName));
									Displayf("%c   bound type %s material %d (of %d) is %s", errCh, pBoundChild->GetTypeString(), i + 1, pBoundChild->GetNumMaterials(), mtlIdName);
								}

								//bOkForSmashTest = false;
							}
						}

						// check for non-smashable glass materials, these should never be used in vehicles
						{
							int unexpectedNonSmashableGlassMaterialCount = 0;

							for (int i = 0; i < pBoundChild->GetNumMaterials(); i++)
							{
								const phMaterialMgr::Id mtlId = PGTAMATERIALMGR->UnpackMtlId(pBoundChild->GetMaterialId(i));

								if (PGTAMATERIALMGR->GetIsGlass(mtlId) && !PGTAMATERIALMGR->GetIsSmashableGlass(mtlId))
								{
									unexpectedNonSmashableGlassMaterialCount++;
								}
							}

							if (unexpectedNonSmashableGlassMaterialCount > 0)
							{
								Displayf("%s unexpected non-smashable glass materials in child bounds ('%s', componentId=%d, type=%d)", vehicleModelDesc, boneName, componentId, pBoundChild->GetType());
								numWarnings++;

								for (int i = 0; i < pBoundChild->GetNumMaterials(); i++)
								{
									const phMaterialMgr::Id mtlId = PGTAMATERIALMGR->UnpackMtlId(pBoundChild->GetMaterialId(i));
									int errCh = ' ';

									if (PGTAMATERIALMGR->GetIsGlass(mtlId) && !PGTAMATERIALMGR->GetIsSmashableGlass(mtlId))
									{
										errCh = '!';
									}

									char mtlIdName[128] = "";
									PGTAMATERIALMGR->GetMaterialName(mtlId, mtlIdName, sizeof(mtlIdName));
									Displayf("%c   material %d (of %d) is non-smashable glass type %s", errCh, i + 1, pBoundChild->GetNumMaterials(), mtlIdName);
								}

								//bOkForSmashTest = false;
							}
						}
					}
					else
					{
						// NULL bound, ok?
					}
				}
			}
		}
	}

	if (1)//g_vehicleGlassSmashTest.m_smashTestBoneNameCheck)
	{
		for (int boneIndex = 0; boneIndex < pVehicle->GetDrawable()->GetSkeletonData()->GetNumBones(); boneIndex++)
		{
			const char* boneName = pVehicle->GetDrawable()->GetSkeletonData()->GetBoneData(boneIndex)->GetName();

			if (stristr(boneName, "win") != NULL && stristr(boneName, "wing") == NULL) // expect "window" or "windscreen", e.g. not "windsceen"
			{
				if (stristr(boneName, "window") == NULL &&
					stristr(boneName, "windscreen") == NULL)
				{
					Displayf("%s bone name '%s' looks like a window, is it misspelled?", vehicleModelDesc, boneName);
					numWarnings++;
				}
				else if (stricmp(boneName, "windscreen_f") == 0)
				{
					Displayf("%s bone name '%s' should be \"windscreen\"", vehicleModelDesc, boneName);
					numWarnings++;
				}
			}
		}
	}

	if (g_vehicleGlassSmashTest.m_smashTestBoneNameMaterialCheck)
	{
		for (int boneIndex = 0; boneIndex < pVehicle->GetDrawable()->GetSkeletonData()->GetNumBones(); boneIndex++)
		{
			const int componentId = pVehicle->GetFragInst()->GetComponentFromBoneIndex(boneIndex); // NOTE -- this only gets the "root" component for this bone index

			if (componentId != -1)
			{
				if (AssertVerify(componentId >= 0 && componentId < pVehicle->GetFragInst()->GetTypePhysics()->GetNumChildren()))
				{
					const fragTypeChild* pFragChild = pVehicle->GetFragInst()->GetTypePhysics()->GetChild(componentId);
					Assert(pVehicle->GetFragInst()->GetType()->GetBoneIndexFromID(pFragChild->GetBoneID()) == boneIndex);

					atArray<phMaterialMgr::Id> mtlIds;
					const phBound* pMaterialBound = GetMaterialIdsFromComponentId(mtlIds, pVehicle, componentId);

					// TODO -- use 'GetMaterialNamesForBone', should be the same .. but i wrote this code later, need to verify both codepaths
					char materialName[1024] = "";
					bool materialIsSmashableGlass = false;

					if (pMaterialBound)
					{
						for (int i = 0; i < mtlIds.GetCount(); i++)
						{
							const phMaterialMgr::Id materialId = PGTAMATERIALMGR->UnpackMtlId(mtlIds[i]);

							if (i > 0)
							{
								strcat(materialName, ",");
							}

							if (PGTAMATERIALMGR->GetIsSmashableGlass(materialId))
							{
								Assert(mtlIds.GetCount() == 1); // smashable glass can't have multiple materials on a single component
								materialIsSmashableGlass = true;
							}

							strcat(materialName, PGTAMATERIALMGR->GetMaterial(materialId).GetName());
						}
					}
					else
					{
						strcpy(materialName, "NULL");
					}

					const char* boneName = pVehicle->GetDrawable()->GetSkeletonData()->GetBoneData(boneIndex)->GetName();
					bool bBoneNameImpliesSmashableGlass = false;
					bool bBoneNameMightBeSmashableGlass = false;

					if (stristr(boneName, "window"     ) ||
						stristr(boneName, "windscreen" ) ||
						stristr(boneName, "sirenglass" ) ||
						stristr(boneName, "siren_glass") ||
						stristr(boneName, "roof_glass" ))
					{
						bBoneNameImpliesSmashableGlass = true;
					}

					if (stristr(boneName, "misc_"))
					{
						bBoneNameMightBeSmashableGlass = true;
					}

					if (materialIsSmashableGlass) // VFXGROUP_CAR_GLASS
					{
						if (!bBoneNameImpliesSmashableGlass && !bBoneNameMightBeSmashableGlass)
						{
							Displayf("%s bone name '%s' does not indicate smashable glass but material is [%s]", vehicleModelDesc, boneName, materialName);
							numWarnings++;
						}
						else if (mtlIds.GetCount() == 1)
						{
							const phMaterialMgr::Id materialId0 = PGTAMATERIALMGR->UnpackMtlId(mtlIds[0]);

							// NOTE -- there may be exceptions to these, e.g. bulletproof vehicles (need to special-case these i guess)
							if (stristr(boneName, "window_"))
							{
								if (materialId0 != PGTAMATERIALMGR->g_idCarGlassWeak)
								{
									Displayf("%s bone name '%s' expects [car_glass_weak] but material is [%s]", vehicleModelDesc, boneName, materialName);
									numWarnings++;
								}
							}
							else if (stricmp(boneName, "windscreen_r") == 0)
							{
								if (materialId0 != PGTAMATERIALMGR->g_idCarGlassMedium)
								{
									Displayf("%s bone name '%s' expects [car_glass_medium] but material is [%s]", vehicleModelDesc, boneName, materialName);
									numWarnings++;
								}
							}
							else if (stristr(boneName, "windscreen")) // either "windscreen" or "windscreen_f", both indicate front windscreen
							{
								if (materialId0 != PGTAMATERIALMGR->g_idCarGlassStrong)
								{
									Displayf("%s bone name '%s' expects [car_glass_strong] but material is [%s]", vehicleModelDesc, boneName, materialName);
									numWarnings++;
								}
							}
						}

						if (AssertVerify(pMaterialBound)) // should never be NULL, since we couldn't have materialIsSmashableGlass if it were
						{
							if (pMaterialBound->GetType() != phBound::GEOMETRY)
							{
								Displayf("%s bone name '%s' physics bound is type %d, expected type GEOMETRY", vehicleModelDesc, boneName, pMaterialBound->GetType());
								numWarnings++;
							}
						}
					}
					else
					{
						if (bBoneNameImpliesSmashableGlass)
						{
							Displayf("%s bone name '%s' indicates smashable glass but material is [%s]", vehicleModelDesc, boneName, materialName);
							numWarnings++;
						}
					}
				}
			}
		}
	}

	return numWarnings;
}

void CVehicleGlassSmashTestManager::CSmashTest::Update()
{
	const CVehicle* pVehicle = NULL;
	const char* vehicleTypeStr = "";
	char vehicleModelDesc[1024] = "";

	int numGroupsSum           = 0;
	int numGroupsMax           = 0;
	int numTrianglesSum        = 0;
	int numTrianglesMax        = 0;
	int numComponentsSmashable = 0;
	int numComponentsSmashed   = 0;

	int compToHierIds[128];
	int componentCount = 0;

	atArray<atString> warnings;
	atArray<atString> warnings_multipleGeoms; // multiple smashable geoms map to the same componentId
	atArray<atString> warnings_disconnected;  // geometry is not connected and/or has backfacing triangles
	atArray<atString> warnings_unexpectedId;  // unexpected hierarchyId for vehicle glass
	atArray<atString> warnings_planeAngle;

	int numWarnings = 0;

	/*
	extern int g_vehglassBoneGlassTriangleCounts[256];
	extern int g_vehglassBoneOtherTriangleCounts[256];
	extern u8  g_vehglassBoneSmashFlags[(256 + 7)/8];

	sysMemSet(g_vehglassBoneGlassTriangleCounts, 0, sizeof(g_vehglassBoneGlassTriangleCounts));
	sysMemSet(g_vehglassBoneOtherTriangleCounts, 0, sizeof(g_vehglassBoneOtherTriangleCounts));
	sysMemSet(g_vehglassBoneSmashFlags, 0, sizeof(g_vehglassBoneSmashFlags));
	*/

	bool bEntityIsNULL = false; // catch the proptrailer, this is NULL for some reason

	for (int i = 0; i < m_collisions.GetCount(); i++)
	{
		const CVehicleGlassComponent* pComponent = g_vehicleGlassMan.ProcessCollision(m_collisions[i], false);

		if (pComponent)
		{
			if (componentCount == 0)
			{
				const CPhysical* pPhysical = pComponent->m_regdPhy.Get();

				if (pPhysical)
				{
					if (pPhysical->GetIsTypeVehicle())
					{
						if (pVehicle)
						{
							Assertf(pVehicle == static_cast<const CVehicle*>(pPhysical), "smash testing two vehicles at once: %s and %s", pVehicle->GetModelName(), pPhysical->GetModelName());
							continue;
						}
						else
						{
							pVehicle = static_cast<const CVehicle*>(pPhysical);
							vehicleTypeStr = GetVehicleTypeStr(pVehicle->GetVehicleType());

							sprintf(vehicleModelDesc, "%s (%s) ", fwArchetype::GetModelName(m_modelIndex), vehicleTypeStr);

							while (strlen(vehicleModelDesc) < 40)
							{
								strcat(vehicleModelDesc, ".");
							}
						}
					}
					else // something's wrong, we shouldn't be smash-testing a non-vehicle
					{
						Assertf(0, "smash testing a non-vehicle entity: %s", pPhysical->GetModelName());
						continue;
					}
				}
				else // something's wrong, why is the entity NULL?
				{
					if (strcmp(CBaseModelInfo::GetModelName(pComponent->m_modelIndex), "proptrailer") != 0) // for some reason this happens every time with proptrailer, ignore it
					{
						Assertf(0, "smash testing a NULL entity: %s", CBaseModelInfo::GetModelName(pComponent->m_modelIndex));
					}

					bEntityIsNULL = true;
					continue;
				}

				GetVehicleComponentToHierarchyMapping(pVehicle, compToHierIds);

				const phBound* pBound = pVehicle->GetFragInst()->GetArchetype()->GetBound();
				componentCount = (pBound->GetType() == phBound::COMPOSITE) ? static_cast<const phBoundComposite*>(pBound)->GetNumBounds() : 1;

				Assert(componentCount > 0);
			}

			int numTriangles = 0;

			int hierarchyId = -1;
			const atString hierarchyIdFullName = GetVehicleHierarchyIdFullName(pVehicle, compToHierIds, pComponent->m_componentId, &hierarchyId);

			// update vehicle hierarchy id counts
			{
				const u32 vehicleNameHash = atStringHash(pVehicle->GetModelName());

				if (g_vehglassHierarchyIdCounts[0] == -1)
				{
					sysMemSet(g_vehglassHierarchyIdCounts, 0, sizeof(g_vehglassHierarchyIdCounts));
				}

				if (g_vehglassHierarchyIdModels.Access(vehicleNameHash) == NULL)
				{
					g_vehglassHierarchyIdModels[vehicleNameHash] = true;

					const CVehicleModelInfo* pVehicleModelInfo = pVehicle->GetVehicleModelInfo();
					const CVehicleStructure* pVehicleStructure = pVehicleModelInfo->GetStructure();

					for (int j = 0; j < VEH_NUM_NODES; j++)
					{
						if (pVehicleStructure->m_nBoneIndices[j] != -1)
						{
							g_vehglassHierarchyIdCounts[j]++;
							g_vehglassHierarchyIdMaxBoneIndex = Max<int>(pVehicleStructure->m_nBoneIndices[j], g_vehglassHierarchyIdMaxBoneIndex);
						}
					}
				}
			}

			const char* boneName = pVehicle->GetDrawable()->GetSkeletonData()->GetBoneData(pComponent->m_boneIndex)->GetName();

			if (!pComponent->m_isValid)
			{
				if (!g_vehicleGlassSmashTest.m_smashTestTexcoordBoundsCheckOnly)
				{
					Displayf("%s no smashable glass groups created for '%s' componentId=%d", vehicleModelDesc, boneName, pComponent->m_componentId);
				}

				numWarnings++;
			}
			else
			{
/*#if 0
				Vec4V texcoordMin(V_FLT_MAX);
				Vec4V texcoordMax = -texcoordMin;

				for (const CVehicleGlassTriangle* pTriangle = pComponent->m_triangleList.GetHead(); pTriangle; pTriangle = pComponent->m_triangleList.GetNext(pTriangle))
				{
					for (int k = 0; k < 3; k++)
					{
						const Vec4V texcoord = pTriangle->GetVertexT(k);

						texcoordMin = Min(texcoord, texcoordMin);
						texcoordMax = Max(texcoord, texcoordMax);
					}

					numTriangles++;
				}

				g_vehglassTexcoordMin = Min(texcoordMin, g_vehglassTexcoordMin);
				g_vehglassTexcoordMax = Max(texcoordMax, g_vehglassTexcoordMax);

				const float texcoordMarginErr = g_vehicleGlassSmashTest.m_smashTestTexcoordBoundsCheckOnly ? 0.0f : 0.07f; // texcoords outside [0..1] by more than this amount will be reported

				if (MinElement(texcoordMin).Getf() < 0.0f - texcoordMarginErr ||
					MaxElement(texcoordMax).Getf() > 1.0f + texcoordMarginErr)
				{
					Displayf(
						"%s '%s' componentId=%d texcoord bounds [%f %f %f %f]..[%f %f %f %f],primary err=,%f,secondary err=,%f", // csv-compatible format
						vehicleModelDesc,
						boneName,
						pComponent->m_componentId,
						VEC4V_ARGS(texcoordMin),
						VEC4V_ARGS(texcoordMax),
						Max<float>(0.0f, -Min<float>(texcoordMin.GetXf(), texcoordMin.GetYf()), Max<float>(texcoordMax.GetXf(), texcoordMax.GetYf()) - 1.0f),
						Max<float>(0.0f, -Min<float>(texcoordMin.GetZf(), texcoordMin.GetWf()), Max<float>(texcoordMax.GetZf(), texcoordMax.GetWf()) - 1.0f)
					);

					if (g_vehicleGlassSmashTest.m_smashTestTexcoordBoundsCheckOnly)
					{
						continue;
					}
				}

				// disabling this code for now .. it's not producing useful results
#define VEHICLE_GLASS_USE_SECONDARY_UVS_TEST (0)

				if (VEHICLE_GLASS_USE_SECONDARY_UVS_TEST)
				{
					const bool bDoSecondaryUVTest = true; // TODO -- hook up to rag

					Vec3V sSum = Vec3V(V_ZERO);
					Vec3V tSum = Vec3V(V_ZERO);
					Vec3V sMin = Vec3V(V_FLT_MAX);
					Vec3V tMin = Vec3V(V_FLT_MAX);
					Vec3V sMax = -Vec3V(V_FLT_MAX);
					Vec3V tMax = -Vec3V(V_FLT_MAX);
					ScalarV sMagSqrSum = ScalarV(V_ZERO);
					ScalarV tMagSqrSum = ScalarV(V_ZERO);
					ScalarV sMagSqrMin = ScalarV(V_FLT_MAX);
					ScalarV tMagSqrMin = ScalarV(V_FLT_MAX);
					ScalarV sMagSqrMax = -ScalarV(V_FLT_MAX);
					ScalarV tMagSqrMax = -ScalarV(V_FLT_MAX);
					ScalarV stSkewMin = ScalarV(V_FLT_MAX);
					ScalarV stSkewMax = -ScalarV(V_FLT_MAX);

					fwBestFitPlane bfp;

					Vec4V   plane = Vec4V(V_ZERO);
					ScalarV planeMaxAbsDist = ScalarV(V_ZERO);

					if (bDoSecondaryUVTest)
					{
						for (const CVehicleGlassTriangle* pTriangle = pComponent->m_triangleList.GetHead(); pTriangle; pTriangle = pComponent->m_triangleList.GetNext(pTriangle))
						{
							// TODO -- should we use all the vertices, or just the boundary edges?
							for (int k = 0; k < 3; k++)
							{
								bfp.AddPoint(pTriangle->GetVertexP(k));
							}
						}

						plane = bfp.GetPlane();

						for (const CVehicleGlassTriangle* pTriangle = pComponent->m_triangleList.GetHead(); pTriangle; pTriangle = pComponent->m_triangleList.GetNext(pTriangle))
						{
							Vec3V s;
							Vec3V t;
							const bool bOk = CalculateTriangleTangentSpace(
								s,
								t,
								pTriangle->GetVertexP(0),
								pTriangle->GetVertexP(1),
								pTriangle->GetVertexP(2),
								pTriangle->GetVertexT(0).GetZW(),
								pTriangle->GetVertexT(1).GetZW(),
								pTriangle->GetVertexT(2).GetZW()
							);

							if (!bOk)
							{
								continue;
							}

							const ScalarV sMagSqr = MagSquared(s);
							const ScalarV tMagSqr = MagSquared(t);

							sSum += s;
							tSum += t;
							sMin = Min(s, sMin);
							tMin = Min(t, tMin);
							sMax = Max(s, sMax);
							tMax = Max(t, tMax);
							sMagSqrSum += sMagSqr;
							tMagSqrSum += tMagSqr;
							sMagSqrMin = Min(sMagSqr, sMagSqrMin);
							tMagSqrMin = Min(tMagSqr, tMagSqrMin);
							sMagSqrMax = Max(sMagSqr, sMagSqrMax);
							tMagSqrMax = Max(tMagSqr, tMagSqrMax);

							if (Min(sMagSqr, tMagSqr).Getf() > 0.000001f)
							{
								const ScalarV stSkew = Dot(Normalize(s), Normalize(t));

								stSkewMin = Min(stSkew, stSkewMin);
								stSkewMax = Max(stSkew, stSkewMax);
							}

							planeMaxAbsDist = Max(Abs(PlaneDistanceTo(plane, pTriangle->GetVertexP(0))), planeMaxAbsDist);
							planeMaxAbsDist = Max(Abs(PlaneDistanceTo(plane, pTriangle->GetVertexP(1))), planeMaxAbsDist);
							planeMaxAbsDist = Max(Abs(PlaneDistanceTo(plane, pTriangle->GetVertexP(2))), planeMaxAbsDist);
						}

						sSum *= ScalarV(1.0f/(float)numTriangles);
						tSum *= ScalarV(1.0f/(float)numTriangles);
						sMagSqrSum *= ScalarV(1.0f/(float)numTriangles);
						tMagSqrSum *= ScalarV(1.0f/(float)numTriangles);

						const Vec3V sDir = Normalize(sSum);
						const Vec3V tDir = Normalize(tSum);

						ScalarV sMaxAbsSkew = ScalarV(V_ZERO);
						ScalarV tMaxAbsSkew = ScalarV(V_ZERO);

						for (const CVehicleGlassTriangle* pTriangle = pComponent->m_triangleList.GetHead(); pTriangle; pTriangle = pComponent->m_triangleList.GetNext(pTriangle))
						{
							Vec3V s;
							Vec3V t;
							const bool bOk = CalculateTriangleTangentSpace(
								s,
								t,
								pTriangle->GetVertexP(0),
								pTriangle->GetVertexP(1),
								pTriangle->GetVertexP(2),
								pTriangle->GetVertexT(0).GetZW(),
								pTriangle->GetVertexT(1).GetZW(),
								pTriangle->GetVertexT(2).GetZW()
							);

							if (!bOk)
							{
								continue;
							}

							if (MagSquared(s).Getf() > 0.000001f)
							{
								sMaxAbsSkew = Max(Abs(Dot(sDir, Normalize(s))), sMaxAbsSkew);
							}

							if (MagSquared(t).Getf() > 0.000001f)
							{
								tMaxAbsSkew = Max(Abs(Dot(tDir, Normalize(t))), tMaxAbsSkew);
							}
						}

						if (planeMaxAbsDist.Getf() > 0.05f) // more than 5cm indicates a "curved" window, we'll have to evaluate these visually somehow
						{
							Displayf("%s %s window is curved (dist=%f)", vehicleModelDesc, boneName, planeMaxAbsDist.Getf());
						}
						else
						{
							//const float sMaxAbsAng = acosf(sMaxAbsSkew.Getf())*RtoD;
							//const float tMaxAbsAng = acosf(tMaxAbsSkew.Getf())*RtoD;
							//const float stAngMin = acosf(stSkewMin.Getf())*RtoD;
							//const float stAngMax = acosf(stSkewMax.Getf())*RtoD;
							//if (Max<float>(sMaxAbsAng, tMaxAbsAng) > 5.0f ||
							//	Max<float>(Abs<float>(stAngMin), Abs<float>(stAngMax)) > 5.0f ||
							//	0)
							{
								Displayf("%s secondary UV test: '%s'", vehicleModelDesc, boneName);

								Displayf(
									"  s: dir=%f,%f,%f, avg=%f,%f,%f, min=%f,%f,%f, max=%f,%f,%f, magsqr avg=%f, min=%f, max=%f, maxabsskew=%f",
									VEC3V_ARGS(sDir),
									VEC3V_ARGS(sSum),
									VEC3V_ARGS(sMin),
									VEC3V_ARGS(sMax),
									sqrtf(sMagSqrSum.Getf()),
									sqrtf(sMagSqrMin.Getf()),
									sqrtf(sMagSqrMax.Getf()),
									acosf(sMaxAbsSkew.Getf())*RtoD
								);

								Displayf(
									"  t: dir=%f,%f,%f, avg=%f,%f,%f, min=%f,%f,%f, max=%f,%f,%f, magsqr avg=%f, min=%f, max=%f, maxabsskew=%f",
									VEC3V_ARGS(tDir),
									VEC3V_ARGS(tSum),
									VEC3V_ARGS(tMin),
									VEC3V_ARGS(tMax),
									sqrtf(tMagSqrSum.Getf()),
									sqrtf(tMagSqrMin.Getf()),
									sqrtf(tMagSqrMax.Getf()),
									acosf(tMaxAbsSkew.Getf())*RtoD
								);

								Displayf("  skew: min=%f, max=%f", acosf(stSkewMin.Getf())*RtoD, acosf(stSkewMax.Getf())*RtoD);
								Displayf("  dist: %f", planeMaxAbsDist.Getf());
							}
						}
					}
				}
#endif*/ // 0
			}

			numGroupsSum += 1;
			numGroupsMax = Max<int>(1, numGroupsMax); // TODO -- remove this
			numTrianglesSum += numTriangles;
			numTrianglesMax = Max<int>(numTriangles, numTrianglesMax);

			/*
			if (pComponent->m_materialId == PGTAMATERIALMGR->g_idCarGlassBulletproof) // some vehicles have multiple bulletproof windows attached to the same component, we don't care if they're disconnected i guess
			{
				warnings.PushAndGrow(atVarString("  %s has %d groups", boneName, numGroups));//, pComponent->m_smashTestNumTrianglesBackface));
				warnings_disconnected.PushAndGrow(atString(boneName));
				numWarnings++;
			}
			*/

			if ((hierarchyId >= VEH_FIRST_WINDOW  && hierarchyId <= VEH_LAST_WINDOW) ||
				(hierarchyId >= VEH_MISC_A        && hierarchyId <= VEH_LAST_MISC) ||
				(hierarchyId >= VEH_SIREN_GLASS_1 && hierarchyId <= VEH_SIREN_GLASS_MAX))
			{
				// ok
			}
			else if (hierarchyId != -1 || g_vehicleGlassSmashTest.m_smashTestUnexpectedHierarchyIds) // unfortunately there are often glass components with no hierarchyId .. so don't report them
			{
				warnings.PushAndGrow(atVarString("  %s is unexpected for vehicle glass", boneName));
				warnings_unexpectedId.PushAndGrow(atString(boneName));
				numWarnings++;
			}

			if (1) // check boneIndex/componentId mapping (BS#829377)
			{
				fragInst* pFragInst = pVehicle->GetFragInst();

				if (AssertVerify(pFragInst && pFragInst == pVehicle->GetFragInst()))
				{
					const int groupId = pFragInst->GetGroupFromBoneIndex(pComponent->m_boneIndex);
					bool bMultipleComponents = false;

					if (groupId != -1)
					{
						const fragTypeGroup* pGroup = pFragInst->GetTypePhysics()->GetGroup(groupId);

						if (pGroup->GetNumChildren() != 1)
						{
							const char* boneName = pVehicle->GetDrawable()->GetSkeletonData()->GetBoneData(pComponent->m_boneIndex)->GetName();

							Displayf("%s bone '%s' (index=%d, group=%d) maps to multiple components [%d..%d]", vehicleModelDesc, boneName, pComponent->m_boneIndex, groupId, pGroup->GetChildFragmentIndex(), pGroup->GetChildFragmentIndex() + pGroup->GetNumChildren() - 1);
							bMultipleComponents = true;
						}
					}

					if (!bMultipleComponents)
					{
						const int expectedComponentId = pFragInst->GetComponentFromBoneIndex(pComponent->m_boneIndex); // NOTE -- this only gets the "root" component for this bone index

						if (pComponent->m_componentId != expectedComponentId)
						{
							const char* boneName = pVehicle->GetDrawable()->GetSkeletonData()->GetBoneData(pComponent->m_boneIndex)->GetName();

							Displayf("%s bone '%s' (index=%d) maps to componentId %d, not %d as expected", vehicleModelDesc, boneName, pComponent->m_boneIndex, expectedComponentId, pComponent->m_componentId);
						}
					}
				}
			}

			numComponentsSmashed++;
		}

		numComponentsSmashable++;
	}

	if (pVehicle == NULL)
	{
		Assertf(bEntityIsNULL, "%s: smash test failed", CBaseModelInfo::GetModelName(m_modelIndex));
		return;
	}

	// not sure why some of these don't have a drawable, let's report it ..
	{
		if (pVehicle->GetDrawable() == NULL)
		{
			Assertf(0, "%s: no drawable??", pVehicle->GetModelName());
			return;
		}
		else if (pVehicle->GetDrawable()->GetSkeletonData() == NULL)
		{
			Assertf(0, "%s: no skeleton data??", pVehicle->GetModelName());
			return;
		}
	}

	if (g_vehicleGlassSmashTest.m_smashTestTexcoordBoundsCheckOnly)
	{
		return;
	}

	numWarnings += CheckVehicle(pVehicle);

	class GetMaterialNamesForBone { public: static void func(char* buf, const CVehicle* pVehicle, int boneIndex)
	{
		const fragInst* pFragInst = pVehicle->GetFragInst();
		const fragType* pFragType = pFragInst->GetType();
		const phBound* pBound = pFragInst->GetArchetype()->GetBound();
		const int componentCount = (pBound->GetType() == phBound::COMPOSITE) ? static_cast<const phBoundComposite*>(pBound)->GetNumBounds() : 1;

		for (int componentId = 0; componentId < componentCount; componentId++)
		{
			const fragTypeChild* pFragChild = pFragInst->GetTypePhysics()->GetChild(componentId);

			if (AssertVerify(pBound->GetType() == phBound::COMPOSITE) && pFragType->GetBoneIndexFromID(pFragChild->GetBoneID()) == boneIndex)
			{
				const phBound* pBoundChild = static_cast<const phBoundComposite*>(pBound)->GetBound(componentId);

				if (pBoundChild)// && pBoundChild->GetType() == phBound::GEOMETRY)
				{
					for (int j = 0; j < pBoundChild->GetNumMaterials(); j++)
					{
						const phMaterialMgr::Id mtlId = PGTAMATERIALMGR->UnpackMtlId(pBoundChild->GetMaterialId(j));
						char mtlIdName[128] = "";
						PGTAMATERIALMGR->GetMaterialName(mtlId, mtlIdName, sizeof(mtlIdName));

						if (buf[0] != '\0')
						{
							strcat(buf, ",");
						}

						strcat(buf, mtlIdName);

						while (isdigit(buf[strlen(buf) - 1])) // strip trailing digits and underscore
						{
							buf[strlen(buf) - 1] = '\0';

							if (buf[strlen(buf) - 1] == '_')
							{
								buf[strlen(buf) - 1] = '\0';
								break;
							}
						}
					}
				}
				else
				{
					strcpy(buf, "NULL");
				}
			}
		}
	}};

	if (g_vehicleGlassSmashTest.m_smashTestCompactOutput)
	{
		/*
		const eHierarchyId* vehicleDamageGlassHierarchyIds = CVehicleDamage::GetGlassBoneHierarchyIds();
		const int vehicleDamageGlassHierarchyIdCount = CVehicleDamage::GetGlassBoneHierarchyIdCount();

		for (int boneIndex = 0; boneIndex < NELEM(g_vehglassBoneGlassTriangleCounts); boneIndex++)
		{
			if (g_vehglassBoneSmashFlags[boneIndex/8] & BIT(boneIndex%8))
			{
				// let's not report non-glass triangles attached to smashable surfaces for now, i think this is ok
			}
			else if (g_vehicleGlassSmashTest.m_smashTestReportSmashableGlassOnAnyNonSmashableMaterials) // not smashed, so it should not contain any smashable glass
			{
				if (g_vehglassBoneGlassTriangleCounts[boneIndex] > 0)
				{
					bool bIgnore = false;

					for (int i = 0; i < vehicleDamageGlassHierarchyIdCount; i++)
					{
						if (boneIndex == pVehicle->GetBoneIndex(vehicleDamageGlassHierarchyIds[i]))
						{
							bIgnore = true;
							break;
						}
					}

					if (!bIgnore)
					{
						for (int sirenId = VEH_SIREN_1; sirenId <= VEH_SIREN_MAX; sirenId++)
						{
							if (boneIndex == pVehicle->GetBoneIndex((eHierarchyId)sirenId))
							{
								bIgnore = true;
								break;
							}
						}
					}

					if (!bIgnore)
					{
						char materialNames[1024] = "";
						GetMaterialNamesForBone::func(materialNames, pVehicle, boneIndex);
						const char* boneName = pVehicle->GetDrawable()->GetSkeletonData()->GetBoneData(boneIndex)->GetName();

						// note that windows and windscreens should be smashable glass, but other bones (e.g. headlights) might not be
						if (stristr(boneName, "window"     ) ||
							stristr(boneName, "windscreen" ) ||
						//	stristr(boneName, "sirenglass" ) ||
						//	stristr(boneName, "siren_glass") ||
							stristr(boneName, "roof_glass" ) ||
							g_vehicleGlassSmashTest.m_smashTestReportSmashableGlassOnAllNonSmashableMaterials)
						{
							Displayf("%s found %d smashable glass triangles attached to non-smashable bone '%s' material [%s]", vehicleModelDesc, g_vehglassBoneGlassTriangleCounts[boneIndex], boneName, materialNames);
							numWarnings++;
						}
					}
				}
			}
		}
		*/

		if (numWarnings > 0)
		{
			char line[1024] = "";

			if (warnings_multipleGeoms.GetCount() > 0)
			{
				line[0] = '\0'; for (int i = 0; i < warnings_multipleGeoms.GetCount(); i++) { if (i > 0) { strcat(line, ","); } strcat(line, warnings_multipleGeoms[i].c_str()); }
				Displayf("%s multiple glass geometries in [%s], %d ok", vehicleModelDesc, line, numComponentsSmashed - warnings_multipleGeoms.GetCount());
			}

			if (warnings_disconnected.GetCount() > 0)
			{
				line[0] = '\0'; for (int i = 0; i < warnings_disconnected.GetCount(); i++) { if (i > 0) { strcat(line, ","); } strcat(line, warnings_disconnected[i].c_str()); }
				Displayf("%s disconnected/backfacing glass geometry in [%s], %d ok", vehicleModelDesc, line, numComponentsSmashed - warnings_disconnected.GetCount());
			}

			if (warnings_unexpectedId.GetCount() > 0)
			{
				line[0] = '\0'; for (int i = 0; i < warnings_unexpectedId.GetCount(); i++) { if (i > 0) { strcat(line, ","); } strcat(line, warnings_unexpectedId[i].c_str()); }
				Displayf("%s unexpected glass hierarchyId in [%s], %d ok", vehicleModelDesc, line, numComponentsSmashed - warnings_unexpectedId.GetCount());
			}

			// NOTE -- disabling this warning for now .. need to revisit this later
			if (0 && warnings_planeAngle.GetCount() > 0)
			{
				line[0] = '\0'; for (int i = 0; i < warnings_planeAngle.GetCount(); i++) { if (i > 0) { strcat(line, ","); } strcat(line, warnings_planeAngle[i].c_str()); }
				Displayf("%s window data plane angle too high in [%s], %d ok", vehicleModelDesc, line, numComponentsSmashed - warnings_planeAngle.GetCount());
			}
		}
		else if (g_vehicleGlassSmashTest.m_smashTestReportOk)
		{
			Displayf("%s ok", vehicleModelDesc);
		}
	}
	else
	{
		Displayf(
			"smashed %d%s of %d components producing max=%d avg=%.2f groups/component and max=%d avg=%.2f triangles/component on %s (%s)",
			numComponentsSmashed,
			numComponentsSmashed < numComponentsSmashable ? atVarString("/%d", numComponentsSmashable).c_str() : "",
			componentCount,
			numGroupsMax,
			(float)numGroupsSum/(float)numComponentsSmashed,
			numTrianglesMax,
			(float)numTrianglesSum/(float)numGroupsSum,
			fwArchetype::GetModelName(m_modelIndex),
			vehicleTypeStr
		);

		for (int i = 0; i < warnings.GetCount(); i++)
		{
			Displayf("%s", warnings[i].c_str());
		}

#if VEHICLE_GLASS_VERBOSE_DEBUG_OUTPUT
		if (pVehicle && g_vehicleGlassMan.m_verbose)
		{
			GetVehicleComponentToHierarchyMapping(pVehicle, NULL, true); // just report warnings

			/*for (int boneIndex = 0; boneIndex < NELEM(g_vehglassBoneGlassTriangleCounts); boneIndex++)
			{
				if (g_vehglassBoneSmashFlags[boneIndex/8] & BIT(boneIndex%8)) // this bone has been smashed, so it should be glass-only
				{
					if (g_vehglassBoneOtherTriangleCounts[boneIndex] > 0)
					{
						char materialNames[1024] = "";
						GetMaterialNamesForBone::func(materialNames, pVehicle, boneIndex);
						const char* boneName = pVehicle->GetDrawable()->GetSkeletonData()->GetBoneData(boneIndex)->GetName();
						Displayf("### %s: found %d non-smashable glass triangles attached to smashed bone '%s' material [%s]", pVehicle->GetModelName(), g_vehglassBoneOtherTriangleCounts[boneIndex], boneName, materialNames);
					}
				}
				else // not smashed, so it should not contain any smashable glass
				{
					if (g_vehglassBoneGlassTriangleCounts[boneIndex] > 0)
					{
						char materialNames[1024] = "";
						GetMaterialNamesForBone::func(materialNames, pVehicle, boneIndex);
						const char* boneName = pVehicle->GetDrawable()->GetSkeletonData()->GetBoneData(boneIndex)->GetName();
						Displayf("### %s: found %d smashable glass triangles attached to non-smashed bone '%s' material [%s]", pVehicle->GetModelName(), g_vehglassBoneGlassTriangleCounts[boneIndex], boneName, materialNames);
					}
				}
			}*/
		}
#endif // VEHICLE_GLASS_VERBOSE_DEBUG_OUTPUT
	}
}

void CVehicleGlassSmashTestManager::ShutdownComponent(CVehicleGlassComponent* pComponent)
{
#if VEHICLE_GLASS_VERBOSE_DEBUG_OUTPUT
	if (!m_smashTest && g_vehicleGlassMan.m_verbose)
	{
		const CPhysical* pPhysical = pComponent->m_regdPhy.Get();

		if (pPhysical)
		{
			if (pPhysical->GetDrawable())
			{
				const char* boneName = pPhysical->GetDrawable()->GetSkeletonData()->GetBoneData(pComponent->m_boneIndex)->GetName();

				Displayf("%s: removing vehicle glass for componentId=%d, bone='%s'", pPhysical->GetModelName(), pComponent->m_componentId, boneName);
			}
			else
			{
				Displayf("%s: removing vehicle glass for componentId=%d, drawable is NULL", pPhysical->GetModelName(), pComponent->m_componentId);
			}
		}
		else
		{
			Displayf("%s: removing vehicle glass for componentId=%d", "NULL", pComponent->m_componentId);
		}
	}
#endif // VEHICLE_GLASS_VERBOSE_DEBUG_OUTPUT
}

void VehicleGlassSmashTest(CVehicle* pNewVehicle)
{
	g_vehicleGlassSmashTest.SmashTest(pNewVehicle);
}

#endif // VEHICLE_GLASS_SMASH_TEST
