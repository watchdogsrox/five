// netScriptMgrBase.cpp
//
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved.
//

#include "script/handlers/GameScriptMgr.h"

// rage includes
#include "bank/bank.h"
#include "grcore/debugdraw.h"
#include "fwsys/timer.h"
#include "fwscript/scriptguid.h"

// game includes
#include "modelinfo/PedModelInfo.h"
#include "modelinfo/VehicleModelInfo.h"
#include "network/Arrays/NetworkArrayMgr.h"
#include "network/Bandwidth/NetworkBandwidthManager.h"
#include "network/Events/NetworkEventTypes.h"
#include "network/NetworkInterface.h"
#include "network/Network.h"
#include "network/Objects/NetworkObjectMgr.h"
#include "network/Players/NetGamePlayer.h"
#include "network/Sessions/NetworkSession.h"
#include "peds/ped.h"
#include "peds/PlayerInfo.h"
#include "scene/streamer/StreamVolume.h"
#include "scene/world/GameWorld.h"
#include "script/handlers/GameScriptEntity.h"
#include "script/handlers/GameScriptHandler.h"
#include "script/handlers/GameScriptHandlerNetwork.h"
#include "script/handlers/GameScriptResources.h"
#include "script/gta_thread.h"
#include "script/script.h"
#include "script/script_channel.h"
#include "script/streamedscripts.h"
#include "streaming/populationstreaming.h"
#include "streaming/streamingdebug.h"
#include "streaming/streamingengine.h"
#include "streaming/streaminginfo.h"
#include "vfx/misc/MovieManager.h"
#include "Event/EventGroup.h"

#if __DEV
#include "network/NetworkInterface.h"
#include "network/objects/NetworkObjectMgr.h"
#include "network/Players/NetworkPlayerMgr.h"
#include "rline/rlgamerinfo.h"
#endif

SCRIPT_OPTIMISATIONS()
NETWORK_OPTIMISATIONS()

XPARAM(override_script);
XPARAM(dontCrashOnScriptValidation);

PARAM(scriptLogMemoryPeaks, "[MEMORY] Log the memory peaks for the scripts physical and virtual usage.");

#if __BANK
bkCombo*	  CGameScriptHandlerMgr::ms_pHandlerCombo = NULL;
const char*   CGameScriptHandlerMgr::ms_Handlers[CGameScriptHandler::MAX_NUM_SCRIPT_HANDLERS];
unsigned int  CGameScriptHandlerMgr::ms_NumHandlers = 0;
int           CGameScriptHandlerMgr::ms_HandlerComboIndex = 0;
bool		  CGameScriptHandlerMgr::ms_DisplayHandlerInfo = false;
bool		  CGameScriptHandlerMgr::ms_DisplayAllMPHandlers = false;
#endif // __BANK

u32 CGameScriptHandlerMgr::ms_validNetworkScriptHashes[] =
{
	ATSTRINGHASH("act_cinema", 0x8961d933),
	ATSTRINGHASH("activity_creator_prototype_launcher", 0x4fb9c566),
	ATSTRINGHASH("am_agency_suv", 0x12117cf0),
	ATSTRINGHASH("am_airstrike", 0x060cd733),
	ATSTRINGHASH("am_ammo_drop", 0xa7c6571d),
	ATSTRINGHASH("am_armwrestling", 0x3c77f4aa),
	ATSTRINGHASH("am_armwrestling_apartment", 0x69bdb2d8),
	ATSTRINGHASH("am_armybase", 0x4efb7452),
	ATSTRINGHASH("am_backup_heli", 0xddb3aada),
	ATSTRINGHASH("am_beach_washup_cinematic", 0xE720B4A4),
	ATSTRINGHASH("am_boat_taxi", 0xd8133249),
	ATSTRINGHASH("am_bru_box", 0x48600c01),
	ATSTRINGHASH("am_car_mod_tut", 0x83a66932),
	ATSTRINGHASH("am_casino_limo", 0x3A5BBA05),
	ATSTRINGHASH("am_casino_luxury_car", 0xb93135a6),
	ATSTRINGHASH("am_casino_peds", 0x1C0F96E7),
	ATSTRINGHASH("am_challenges", 0x59de0357),
	ATSTRINGHASH("am_cp_collection", 0x0544b90b),
	ATSTRINGHASH("am_crate_drop", 0x240f2e7b),
	ATSTRINGHASH("am_criminal_damage", 0xc62526f5),
	ATSTRINGHASH("am_darts", 0x2a10cf0c),
	ATSTRINGHASH("am_darts_apartment", 0xab14f1f2),
	ATSTRINGHASH("am_dead_drop", 0x9497de5d),
	ATSTRINGHASH("am_destroy_veh", 0xeab28126),
	ATSTRINGHASH("am_distract_cops", 0x81dc676e),	
	ATSTRINGHASH("am_doors", 0xc80b3112),
	ATSTRINGHASH("am_ferriswheel", 0xe4e0cbce),
	ATSTRINGHASH("am_ga_pickups", 0xa77e5305),
	ATSTRINGHASH("am_gang_call", 0x0266ce78),
	ATSTRINGHASH("am_heist_int", 0xc74cc971),
	ATSTRINGHASH("am_heist_island_plane_land_cinematic", 0x560E198),
	ATSTRINGHASH("am_heist_island_plane_take_off_cinematic", 0x1984610C),
	ATSTRINGHASH("am_heli_taxi", 0x4840fef0),
	ATSTRINGHASH("am_hi_plane_take_off_cinematic", 0x418795CC),
	ATSTRINGHASH("am_hi_plane_land_cinematic", 0x9782808A),
	ATSTRINGHASH("am_hold_up", 0x0c6dac46),
	ATSTRINGHASH("am_hot_property", 0x95335193),
	ATSTRINGHASH("am_hot_target", 0x3c04751e),
	ATSTRINGHASH("am_hs4_isd_take_vel", 0x9B9330A1),
	ATSTRINGHASH("am_hs4_lsa_land_nimb_arrive",0x2941F161),
	ATSTRINGHASH("am_hs4_lsa_land_vel", 0xC288DE3E),
	ATSTRINGHASH("am_hs4_lsa_take_vel", 0x7BD40072),
	ATSTRINGHASH("am_hs4_nimb_isd_lsa_leave", 0x85398B65),
	ATSTRINGHASH("am_hs4_nimb_lsa_isd_leave", 0xC2F73739),
	ATSTRINGHASH("am_hs4_nimb_lsa_isd_arrive", 0x39DB7A9C),
	ATSTRINGHASH("am_hs4_vel_lsa_isd", 0x1FF4E6A9),
	ATSTRINGHASH("am_hunt_the_beast", 0x7e76f859),
	ATSTRINGHASH("am_lsia_land_cinematic", 0xAEA5F533),
	ATSTRINGHASH("am_imp_exp", 0x1961b859),
	ATSTRINGHASH("am_island_backup_heli", 0x68867AF7),
	ATSTRINGHASH("am_joyrider", 0xe1bc7b99),
	ATSTRINGHASH("am_kill_list", 0xa8d5e84c),
	ATSTRINGHASH("am_king_of_the_castle", 0x7a9a3349),
	ATSTRINGHASH("am_launcher", 0x57c919e1),
	ATSTRINGHASH("am_lester_cut", 0x1edd4ad1),
	ATSTRINGHASH("am_lowrider_int", 0x4250943e),
	ATSTRINGHASH("am_lsia_take_off_cinematic", 0x849DBDF1),
	ATSTRINGHASH("am_luxury_showroom", 0xf4ab6fb5),
	ATSTRINGHASH("am_mission_launch", 0x8fa27332),
	ATSTRINGHASH("am_mp_acid_lab", 0x480e7a1a),
	ATSTRINGHASH("am_mp_arc_cab_manager", 0x4ee41f75),
	ATSTRINGHASH("am_mp_arcade", 0x500b732f),
	ATSTRINGHASH("am_mp_arcade_love_meter", 0x39245013),
	ATSTRINGHASH("am_mp_arcade_strength_test", 0x7488fe33),
	ATSTRINGHASH("am_mp_arcade_fortune_teller", 0xf4c6d398),
	ATSTRINGHASH("am_mp_arcade_claw_crane", 0xa54f9693),
	ATSTRINGHASH("am_mp_arena_box", 0x8C9C5FB8),
	ATSTRINGHASH("am_mp_arena_garage", 0x26EBCE20),
	ATSTRINGHASH("am_mp_armory_aircraft", 0x9c6f5716),
	ATSTRINGHASH("am_mp_armory_truck", 0x5b9b0272),
	ATSTRINGHASH("am_mp_auto_shop", 0xba18b4c),
	ATSTRINGHASH("am_mp_biker_warehouse", 0x504ebb57),
	ATSTRINGHASH("am_mp_boardroom_seating", 0x70fc43a2),
	ATSTRINGHASH("am_mp_bunker", 0x89d486f8),
	ATSTRINGHASH("am_mp_business_hub", 0xC85B5BEF),
	ATSTRINGHASH("am_mp_car_meet_property", 0x2385bdc4),
	ATSTRINGHASH("am_mp_car_meet_sandbox", 0x5158A9E4),
	ATSTRINGHASH("am_mp_casino", 0x7166BD4A),
	ATSTRINGHASH("am_mp_casino_apartment", 0xD76D873D),
	ATSTRINGHASH("am_mp_casino_dj_room", 0x12daa5f1),
	ATSTRINGHASH("am_mp_casino_nightclub", 0x700869D),
	ATSTRINGHASH("am_mp_casino_valet_garage", 0x6cc29c20),
	ATSTRINGHASH("am_mp_creator_aircraft", 0xd4dddde0),
	ATSTRINGHASH("am_mp_creator_trailer", 0xcbd05bfb),
	ATSTRINGHASH("am_mp_defunct_base", 0xcab8a943),
	ATSTRINGHASH("am_mp_dj", 0xb369e47f),
	ATSTRINGHASH("am_mp_drone", 0x230D1E0D),
	ATSTRINGHASH("am_mp_fixer_hq", 0x995f4ea5),
	ATSTRINGHASH("am_mp_garage_control", 0xfd4b7439),
	ATSTRINGHASH("am_mp_hacker_truck", 0x349B7463),
	ATSTRINGHASH("am_mp_hangar", 0xefc0d4ec),
	ATSTRINGHASH("am_mp_ie_warehouse", 0xb7a8f8db),
	ATSTRINGHASH("am_mp_island", 0x1C2A2AF8),
	ATSTRINGHASH("am_mp_juggalo_hideout", 0xedfd243e),
	ATSTRINGHASH("am_mp_multistorey_garage", 0x679a06a1),
	ATSTRINGHASH("am_mp_music_studio", 0xfd3397ae),
	ATSTRINGHASH("am_mp_mission", 0x626b9c51),
	ATSTRINGHASH("am_mp_nightclub", 0x8C56B7FE),
	ATSTRINGHASH("am_mp_orbital_cannon", 0x6c25dccb),
	ATSTRINGHASH("am_mp_peds", 0xBC350B6D),
	ATSTRINGHASH("am_mp_property_ext", 0x72ee09f2),
	ATSTRINGHASH("am_mp_property_int", 0x95b61b3c),
	ATSTRINGHASH("am_mp_rc_vehicle", 0xd1894172),
	ATSTRINGHASH("am_mp_shooting_range", 0xaa3368fc),
	ATSTRINGHASH("am_mp_simeon_showroom", 0x1ed5a03d),
	ATSTRINGHASH("am_mp_smoking_activity", 0x121a26b9),
	ATSTRINGHASH("am_mp_smpl_interior_ext", 0xe0a7b7ad),
	ATSTRINGHASH("am_mp_smpl_interior_int", 0x05973129),
	ATSTRINGHASH("am_mp_solomon_office", 0x220A2BF3),
	ATSTRINGHASH("am_mp_submarine", 0xE16957F2),
	ATSTRINGHASH("am_mp_vehicle_reward", 0x692F8E30),
	ATSTRINGHASH("am_mp_vehicle_weapon", 0x322bad51),	
	ATSTRINGHASH("am_mp_warehouse", 0x2262ddf0),
	ATSTRINGHASH("am_mp_yacht", 0xc930c939),
	ATSTRINGHASH("am_npc_invites", 0x529d545f),
	ATSTRINGHASH("am_pass_the_parcel", 0x8dd957aa),
	ATSTRINGHASH("am_patrick", 0xb70b6f22),
	ATSTRINGHASH("am_penned_in", 0x9fd87720),
	ATSTRINGHASH("am_penthouse_peds", 0x5c0238a9),
	ATSTRINGHASH("am_plane_takedown", 0x990a9196),
	ATSTRINGHASH("am_prison", 0x70da15b8),
	ATSTRINGHASH("am_prostitute", 0x62773681),
	ATSTRINGHASH("am_rollercoaster", 0xa8f75ae1),
	ATSTRINGHASH("am_rontrevor_cut", 0xe895e13e),
	ATSTRINGHASH("am_taxi", 0x455fe4de),
	ATSTRINGHASH("am_vehicle_spawn", 0xb726fc82),
	ATSTRINGHASH("apartment_minigame_launcher", 0xc4382362),
	ATSTRINGHASH("arcade_seating", 0xA0B1239B),
	ATSTRINGHASH("arena_box_bench_seats", 0x63905267),
	ATSTRINGHASH("arena_carmod", 0xdafc0911),
	ATSTRINGHASH("arena_workshop_seats", 0x7030f53f),
	ATSTRINGHASH("armory_aircraft_carmod", 0x78ddb08f),
	ATSTRINGHASH("atm_trigger", 0x1ab4168d),
	ATSTRINGHASH("auto_shop_seating", 0x7A4A46AE),
	ATSTRINGHASH("base_carmod", 0x00e813dd),
	ATSTRINGHASH("base_corridor_seats", 0x16833adb),
	ATSTRINGHASH("base_entrance_seats", 0x8d055fe8),
	ATSTRINGHASH("base_heist_seats", 0xc5c0aa16),
	ATSTRINGHASH("base_lounge_seats", 0xddf302ec),
	ATSTRINGHASH("base_quaters_seats", 0x01aa9982),
	ATSTRINGHASH("base_reception_seats", 0x6b938b1b),
	ATSTRINGHASH("beach_exterior_seating", 0x3DE90B7C),
	ATSTRINGHASH("blackjack", 0xD378365E),
	ATSTRINGHASH("business_battles", 0xA6526FA9),
	ATSTRINGHASH("business_battles_defend", 0x6FA239A5),
	ATSTRINGHASH("business_battles_sell", 0xF3F871DA),
	ATSTRINGHASH("business_hub_carmod", 0x6E6F052),
	ATSTRINGHASH("business_hub_garage_seats", 0x7d5dee96),
	ATSTRINGHASH("camhedz_arcade", 0xE81212C6),
	ATSTRINGHASH("car_meet_carmod", 0xBB3D7E9D),
	ATSTRINGHASH("car_meet_exterior_seating", 0x581858D7),
	ATSTRINGHASH("car_meet_interior_seating", 0xAEC3C263),
	ATSTRINGHASH("carmod_shop", 0x1dc6b680),
	ATSTRINGHASH("cashisking", 0xaa69fdd3),
	ATSTRINGHASH("casino_bar_seating", 0x6e6fc95f),
	ATSTRINGHASH("casino_exterior_seating", 0x38C0DADF),
	ATSTRINGHASH("casino_interior_seating", 0xc2c12122),
	ATSTRINGHASH("casino_lucky_wheel", 0xbdf0ccff),
	ATSTRINGHASH("casino_main_lounge_seating", 0x3339c03a),
	ATSTRINGHASH("casino_nightclub_seating", 0xF8881E62),
	ATSTRINGHASH("casino_penthouse_seating", 0x5a1b8571),
	ATSTRINGHASH("casino_slots", 0x5f1459d7),
	ATSTRINGHASH("casinoroulette", 0x05e86d0d),
	ATSTRINGHASH("clothes_shop_mp", 0xc0f0bbc3),
	ATSTRINGHASH("copscrooks_holdup_instance", 0x70871f89),
	ATSTRINGHASH("copscrooks_job_instance", 0xF28DD9D9),
	ATSTRINGHASH("dont_cross_the_line", 0x85db1b5a),
	ATSTRINGHASH("endlesswinter", 0xe237f748),
	ATSTRINGHASH("fixer_hq_carmod", 0x41f74a04),
	ATSTRINGHASH("fixer_hq_seating", 0x1a207399),
	ATSTRINGHASH("fixer_hq_seating_op_floor", 0xc6bd80fc),
	ATSTRINGHASH("fixer_hq_seating_pq", 0x15dac453),
	ATSTRINGHASH("fm_bj_race_controler", 0xbe3e7d03),
	ATSTRINGHASH("fm_content_acid_lab_sell", 0x92772b1f),
	ATSTRINGHASH("fm_content_acid_lab_setup", 0x61ff0c31),
	ATSTRINGHASH("fm_content_acid_lab_source", 0x701beffb),
	ATSTRINGHASH("fm_content_auto_shop_delivery", 0xfc4970dc),
	ATSTRINGHASH("fm_content_ammunation", 0x4537E4CA),
	ATSTRINGHASH("fm_content_bank_shootout", 0x3f21df40),
	ATSTRINGHASH("fm_content_bar_resupply", 0x2363D09D),
	ATSTRINGHASH("fm_content_business_battles", 0x62EA78D0),
	ATSTRINGHASH("fm_content_bike_shop_delivery", 0x3ff69732),
	ATSTRINGHASH("fm_content_cargo", 0x035d0edc),
	ATSTRINGHASH("fm_content_cerberus", 0xe05f49f1),
	ATSTRINGHASH("fm_content_club_management", 0xa14222b3),
	ATSTRINGHASH("fm_content_club_odd_jobs", 0xdf2e4655),
	ATSTRINGHASH("fm_content_club_source", 0x2b88e6a1),
	ATSTRINGHASH("fm_content_clubhouse_contracts", 0x5a98b2be),
	ATSTRINGHASH("fm_content_convoy", 0x4b293c1f),
	ATSTRINGHASH("fm_content_crime_scene", 0x5CA3449B),
	ATSTRINGHASH("fm_content_dispatch", 0xBC0EB82E),
	ATSTRINGHASH("fm_content_drug_lab_work", 0x0bf710af),
	ATSTRINGHASH("fm_content_drug_vehicle", 0x5B3F117),
	ATSTRINGHASH("fm_content_export_cargo", 0xee7c5974),
	ATSTRINGHASH("fm_content_golden_gun", 0xA7CBD213),
	ATSTRINGHASH("fm_content_gunrunning", 0x1a160b48),
	ATSTRINGHASH("fm_content_island_dj", 0xBE590E09),
	ATSTRINGHASH("fm_content_island_heist", 0xF2A65E92),
	ATSTRINGHASH("fm_content_metal_detector", 0xEEDDF72E),
	ATSTRINGHASH("fm_content_movie_props", 0x9BF88E0E),
	ATSTRINGHASH("fm_content_payphone_hit", 0xF7F8D71A),
	ATSTRINGHASH("fm_content_payphone_intro", 0x2F916684),
	ATSTRINGHASH("fm_content_phantom_car", 0xF699F9CD),
	ATSTRINGHASH("fm_content_security_contract", 0x26EA9F66),
	ATSTRINGHASH("fm_content_serve_and_protect", 0xFF78189D),
	ATSTRINGHASH("fm_content_sightseeing", 0x975e5d41),
	ATSTRINGHASH("fm_content_skydive", 0x3d946731),
	ATSTRINGHASH("fm_content_slasher", 0xD45729DB),
	ATSTRINGHASH("fm_content_smuggler_plane", 0xb8140067),
	ATSTRINGHASH("fm_content_smuggler_trail", 0x277b17b6),
	ATSTRINGHASH("fm_content_source_research", 0xe3e84b48),
	ATSTRINGHASH("fm_content_sprint", 0x51C051E7),
	ATSTRINGHASH("fm_content_stash_house", 0x29513dec),
	ATSTRINGHASH("fm_content_taxi_driver", 0xb7782ad4),
	ATSTRINGHASH("fm_content_tuner_robbery", 0xB5278C95),
	ATSTRINGHASH("fm_content_vehicle_list", 0x9E4931D3),
	ATSTRINGHASH("fm_content_vip_contract_1", 0x75C7E0D3),
	ATSTRINGHASH("fm_content_xmas_mugger", 0x6cfc0084),
	ATSTRINGHASH("fm_deathmatch_controler", 0xe62128cb),
	ATSTRINGHASH("fm_hideout_controler", 0x055ede8c),
	ATSTRINGHASH("fm_hold_up_tut", 0x75804856),
	ATSTRINGHASH("fm_horde_controler", 0x514a6d17),
	ATSTRINGHASH("fm_impromptu_dm_controler", 0xad453307),
	ATSTRINGHASH("fm_intro", 0xefe7c380),
	ATSTRINGHASH("fm_mission_controller", 0x6c2ce225),
	ATSTRINGHASH("fm_mission_controller_2020", 0xBF461AD0),
	ATSTRINGHASH("fm_race_controler", 0xeb004e0e),
	ATSTRINGHASH("fm_street_dealer", 0xbc8a3c3e),
	ATSTRINGHASH("fm_survival_controller", 0x1D477306),
	ATSTRINGHASH("fmmc_launcher", 0xa3fb8a5e),
	ATSTRINGHASH("fmmc_playlist_controller", 0x56e10d97),
	ATSTRINGHASH("freemode", 0xc875557d),
	ATSTRINGHASH("freemodearcade", 0xff3cad99),
	ATSTRINGHASH("freemodeLite", 0x1CA0CF36),
	ATSTRINGHASH("gb_airfreight", 0x59e32892),
	ATSTRINGHASH("gb_amphibious_assault", 0x31dc55bb),
	ATSTRINGHASH("gb_assault", 0xc3961a05),
	ATSTRINGHASH("gb_bank_job", 0x929CEFED),
	ATSTRINGHASH("gb_bellybeast", 0xd4d41110),
	ATSTRINGHASH("gb_biker_bad_deal", 0x2334f600),
	ATSTRINGHASH("gb_biker_burn_assets", 0xd9ee6178),
	ATSTRINGHASH("gb_biker_contraband_defend", 0x56a1412c),
	ATSTRINGHASH("gb_biker_contraband_sell", 0xdffc4863),
	ATSTRINGHASH("gb_biker_contract_killing", 0x1c8463b3),
	ATSTRINGHASH("gb_biker_criminal_mischief", 0x3e927a10),
	ATSTRINGHASH("gb_biker_destroy_vans", 0x6a5fc4cc),
	ATSTRINGHASH("gb_biker_driveby_assassin", 0x2898b9e6),
	ATSTRINGHASH("gb_biker_free_prisoner", 0x966afe8f),
	ATSTRINGHASH("gb_biker_joust", 0xa9165795),
	ATSTRINGHASH("gb_biker_last_respects", 0x63389db5),
	ATSTRINGHASH("gb_biker_race_p2p", 0xd4caa546),
	ATSTRINGHASH("gb_biker_rescue_contact", 0x8403149b),
	ATSTRINGHASH("gb_biker_rippin_it_up", 0x64ad4306),
	ATSTRINGHASH("gb_biker_safecracker", 0xfc5f9481),
	ATSTRINGHASH("gb_biker_search_and_destroy", 0x6d5e1016),
	ATSTRINGHASH("gb_biker_shuttle", 0xd41e6264),
	ATSTRINGHASH("gb_biker_stand_your_ground", 0x9e4e7734),
	ATSTRINGHASH("gb_biker_steal_bikes", 0xdd56dc62),
	ATSTRINGHASH("gb_biker_unload_weapons", 0xb31a0796),
	ATSTRINGHASH("gb_biker_wheelie_rider", 0xb3e837ff),
	ATSTRINGHASH("gb_carjacking", 0xa29e8148),
	ATSTRINGHASH("gb_cashing_out", 0xaa79b0eb),
	ATSTRINGHASH("gb_casino", 0x13645DC3),
	ATSTRINGHASH("gb_casino_heist", 0x8ef7e740),
	ATSTRINGHASH("gb_collect_money", 0x206c6c4b),
	ATSTRINGHASH("gb_contraband_buy", 0xeb04de05),
	ATSTRINGHASH("gb_contraband_defend", 0x2055a1a4),
	ATSTRINGHASH("gb_contraband_sell", 0x7b3e31d2),
	ATSTRINGHASH("gb_data_hack", 0x45154B11),
	ATSTRINGHASH("gb_deathmatch", 0x9f42b34b),
	ATSTRINGHASH("gb_delivery", 0x6e9955cb),
	ATSTRINGHASH("gb_finderskeepers", 0xa6320c4d),
	ATSTRINGHASH("gb_fivestar", 0x8dc43a99),
	ATSTRINGHASH("gb_fortified", 0x51925081),
	ATSTRINGHASH("gb_fragile_goods", 0x2382af03),
	ATSTRINGHASH("gb_fully_loaded", 0xda508d57),
	ATSTRINGHASH("gb_gangops", 0xfb30320a),
	ATSTRINGHASH("gb_gang_ops_planning", 0x6d20caab),
	ATSTRINGHASH("gb_gunrunning", 0x8cd0bff0),
	ATSTRINGHASH("gb_gunrunning_defend", 0x14b421aa),
	ATSTRINGHASH("gb_gunrunning_delivery", 0x29a28ed8),
	ATSTRINGHASH("gb_headhunter", 0x71c13b55),
	ATSTRINGHASH("gb_hunt_the_boss", 0x52d21ba4),
	ATSTRINGHASH("gb_ie_delivery_cutscene", 0x9114379b),
	ATSTRINGHASH("gb_illicit_goods_resupply", 0x9d9d13ec),
	ATSTRINGHASH("gb_infiltration", 0x610BE6A0),
	ATSTRINGHASH("gb_jewel_store_grab", 0xB0BCBA60),
	ATSTRINGHASH("gb_ploughed", 0x14611265),
	ATSTRINGHASH("gb_point_to_point", 0x45bd5447),
	ATSTRINGHASH("gb_ramped_up", 0x506cc782),
	ATSTRINGHASH("gb_rob_shop", 0xb4901ab2),
	ATSTRINGHASH("gb_salvage", 0x4eff43a1),
	ATSTRINGHASH("gb_security_van", 0xE8DA8C5F),
	ATSTRINGHASH("gb_sightseer", 0x724eba87),
	ATSTRINGHASH("gb_smuggler", 0xabd3de17),
	ATSTRINGHASH("gb_stockpiling", 0xf60a885b),
	ATSTRINGHASH("gb_target_pursuit", 0x5D94ED7B),
	ATSTRINGHASH("gb_terminate", 0xf2095bfd),
	ATSTRINGHASH("gb_transporter", 0x0b87dc63),
	ATSTRINGHASH("gb_vehicle_export", 0xd9751e61),
	ATSTRINGHASH("gb_velocity", 0x5f6c2123),
	ATSTRINGHASH("gb_yacht_rob", 0x7fa4662e),
	ATSTRINGHASH("ggsm_arcade", 0x2AEDCDFB),
	ATSTRINGHASH("grid_arcade_cabinet", 0xD5597100),
	ATSTRINGHASH("golf_mp", 0xcf654b49),
	ATSTRINGHASH("gunclub_shop", 0x1beb0bf6),
	ATSTRINGHASH("gunslinger_arcade", 0xDB23F3DC),
	ATSTRINGHASH("hacker_truck_carmod", 0xCE27944E),
	ATSTRINGHASH("hairdo_shop_mp", 0xb57685c9),
	ATSTRINGHASH("hangar_carmod", 0x34141a78),
	ATSTRINGHASH("heist_island_planning", 0x55EC87F4),
	ATSTRINGHASH("heistislandtempcinematicfadeout", 0x32ABF33D),
	ATSTRINGHASH("heistislandtempcinematicfadein", 0xC1508DAB),
	ATSTRINGHASH("heli_gun", 0xba023427),
	ATSTRINGHASH("juggalo_hideout_carmod", 0x05760f53),
	ATSTRINGHASH("lobby", 0x5a8cb6b1),
	ATSTRINGHASH("luxe_veh_activity", 0x304094ed),
	ATSTRINGHASH("mg_race_to_point", 0x804da739),
	ATSTRINGHASH("missioniaaturret", 0x0c5e8432),
	ATSTRINGHASH("mp_bed_high", 0x272557eb),
	ATSTRINGHASH("mptestbed", 0x62c09cc6),
	ATSTRINGHASH("mptutorial", 0xa1cb0fb5),
	ATSTRINGHASH("mptutorial old", 0xe79e6d99),
	ATSTRINGHASH("music_studio_seating", 0x95f729db),
	ATSTRINGHASH("music_studio_seating_external", 0xdbabae79),
	ATSTRINGHASH("music_studio_smoking", 0xa5c68a5b),
	ATSTRINGHASH("net_activity_creator_ui", 0x184b4e09),
	ATSTRINGHASH("net_apartment_activity", 0xdd40b31d),
	ATSTRINGHASH("net_apartment_activity_light", 0xf3e2dd79),
	ATSTRINGHASH("net_combat_soaktest", 0xf159fe54),
	ATSTRINGHASH("net_jacking_soaktest", 0x74fa8e62),
	ATSTRINGHASH("net_test_drive", 0x4eb46ff0),
	ATSTRINGHASH("net_session_soaktest", 0xe696faf1),
	ATSTRINGHASH("nightclub_ground_floor_seats", 0x8e55be9d),
	ATSTRINGHASH("nightclub_office_seats", 0xfd519148),
	ATSTRINGHASH("nightclub_vip_seats", 0xdf1e929f),
	ATSTRINGHASH("nightclubpeds", 0x9a821400),
	ATSTRINGHASH("ob_bong", 0x57439d64),
	ATSTRINGHASH("ob_cashregister", 0x0e1819bf),
	ATSTRINGHASH("ob_drinking_shots", 0x000269eb),
	ATSTRINGHASH("ob_franklin_beer", 0xb1b4d0ed),
	ATSTRINGHASH("ob_franklin_wine", 0x00eb926e),
	ATSTRINGHASH("ob_jukebox", 0xD044BED0),
	ATSTRINGHASH("ob_mp_bed_high", 0xe003b72c),
	ATSTRINGHASH("ob_mp_bed_low", 0x0d798ec5),
	ATSTRINGHASH("ob_mp_bed_med", 0x51c89b45),
	ATSTRINGHASH("ob_mp_shower_med", 0x96c72262),
	ATSTRINGHASH("ob_mp_stripper", 0x5a6f3a5f),
	ATSTRINGHASH("ob_telescope", 0x5202cf8c),
	ATSTRINGHASH("ob_vend1", 0xa39c1f72),
	ATSTRINGHASH("ob_vend2", 0x11c77bc7),
	ATSTRINGHASH("pb_prostitute", 0x6a96798a),
	ATSTRINGHASH("personal_carmod_shop", 0xdef103d2),
	ATSTRINGHASH("pilot_school_mp", 0xc5a00edd),
	ATSTRINGHASH("range_modern_mp", 0x1f538c61),
	ATSTRINGHASH("road_arcade", 0x7ae86e42),	
	ATSTRINGHASH("sclub_front_bouncer", 0xe79ae0ef),
	ATSTRINGHASH("simeon_showroom_seating", 0x23802fd8),
	ATSTRINGHASH("specialgarage", 0xbc24c116),
	ATSTRINGHASH("specialgaragevehicles", 0x583f311f),
	ATSTRINGHASH("streaming", 0xd3ac6be7),
	ATSTRINGHASH("stripclub_drinking", 0xede89eb0),
	ATSTRINGHASH("stripclub_mp", 0x97cd8e78),
	ATSTRINGHASH("stripperhome", 0xb8b48b62),
	ATSTRINGHASH("tattoo_shop", 0xc0d26565),
	ATSTRINGHASH("tempofficegarage", 0x738b3a59),
	ATSTRINGHASH("tennis_network_mp", 0xe3940b1c),
	ATSTRINGHASH("testtutorialforproperties", 0x311fd06a),
	ATSTRINGHASH("testtutorialproperties", 0x011e64ab),
	ATSTRINGHASH("testvehiclespawning", 0x2f07d2ae),
	ATSTRINGHASH("testvehiclespawning2", 0x0c093898),
	ATSTRINGHASH("Three_Card_Poker", 0xc8608f32),
	ATSTRINGHASH("tuner_planning", 0x1BE25EB4),
	ATSTRINGHASH("tuner_property_carmod", 0x70b10f84),
	ATSTRINGHASH("tuner_sandbox_activity", 0x8758ef91),
	ATSTRINGHASH("turret_cam_script", 0xe6511181),
	ATSTRINGHASH("vehiclespawning", 0x70f5ddd6),
	ATSTRINGHASH("xml_menus", 0x164e8dee),
};
unsigned CGameScriptHandlerMgr::NUM_VALID_NETWORK_SCRIPTS = (sizeof(CGameScriptHandlerMgr::ms_validNetworkScriptHashes) / sizeof(u32));

CGameScriptHandlerMgr::CGameScriptHandlerMgr(CNetworkBandwidthManager* bandwidthMgr) 
: scriptHandlerMgr(static_cast<netBandwidthMgr*>(bandwidthMgr))
{
}

bool CGameScriptHandlerMgr::CRemoteScriptInfo::IsTerminated() const							
{ 
	unsigned timeout = CNetwork::GetTimeoutTime();

	if (m_ActiveParticipants == 0 && timeout > 0)
	{
		return (gnetVerify(m_TimeInactive != 0) && GetCurrentTime() - m_TimeInactive > (timeout+1000));
	}

	return false;
}

void CGameScriptHandlerMgr::CRemoteScriptInfo::AddActiveParticipant( PhysicalPlayerIndex player )
{ 
	gnetAssert(player < MAX_NUM_PHYSICAL_PLAYERS);

	m_ActiveParticipants |= (1<<player); 
	m_TimeInactive = 0;
}

void CGameScriptHandlerMgr::CRemoteScriptInfo::RemoveParticipant( PhysicalPlayerIndex player )
{ 
	gnetAssert(player < MAX_NUM_PHYSICAL_PLAYERS);

	m_ActiveParticipants &= ~(1<<player); 

	if (m_ActiveParticipants == 0 && m_TimeInactive == 0)
		m_TimeInactive = GetCurrentTime();
}

void CGameScriptHandlerMgr::CRemoteScriptInfo::UpdateInfo( const CRemoteScriptInfo& that )
{
	gnetAssert(that.GetScriptId() == m_ScriptId);

	SetParticipants(that.m_ActiveParticipants);

	m_HostToken			= that.m_HostToken;
	m_Host				= that.m_Host;
	m_ReservedPeds		= that.m_ReservedPeds;
	m_ReservedVehicles	= that.m_ReservedVehicles;
	m_ReservedObjects	= that.m_ReservedObjects;
}

void CGameScriptHandlerMgr::CRemoteScriptInfo::Serialise(CSyncDataBase& serialiser)
{
	static const unsigned SIZEOF_RESERVATION = 7;

	m_ScriptId.Serialise(serialiser);

	SERIALISE_UNSIGNED(serialiser, m_ActiveParticipants,	sizeof(PlayerFlags)<<3,	"Active Participants");
	SERIALISE_UNSIGNED(serialiser, m_HostToken, sizeof(HostToken)<<3,	"Host Token");

	if (m_ActiveParticipants != 0)
	{
		bool bHasReservedPeds = m_ReservedPeds != 0;
		bool bHasReservedVehicles = m_ReservedVehicles != 0;
		bool bHasReservedObjects = m_ReservedObjects != 0;

		SERIALISE_BOOL(serialiser, bHasReservedPeds);
		SERIALISE_BOOL(serialiser, bHasReservedVehicles);
		SERIALISE_BOOL(serialiser, bHasReservedObjects);

		if (bHasReservedPeds || serialiser.GetIsMaximumSizeSerialiser())
		{
			u32 reservedPeds = m_ReservedPeds;
			SERIALISE_UNSIGNED(serialiser, reservedPeds, SIZEOF_RESERVATION, "Reserved peds");
			m_ReservedPeds = (u8) reservedPeds;
		}
		else
		{
			m_ReservedPeds = 0;
		}

		if (bHasReservedVehicles || serialiser.GetIsMaximumSizeSerialiser())
		{
			u32 reservedVehicles = m_ReservedVehicles;
			SERIALISE_UNSIGNED(serialiser, reservedVehicles, SIZEOF_RESERVATION, "Reserved vehicles");
			m_ReservedVehicles = (u8) reservedVehicles;
		}
		else
		{
			m_ReservedVehicles = 0;
		}

		if (bHasReservedObjects || serialiser.GetIsMaximumSizeSerialiser())
		{
			u32 reservedObjects = m_ReservedObjects;
			SERIALISE_UNSIGNED(serialiser, reservedObjects, SIZEOF_RESERVATION, "Reserved objects");
			m_ReservedObjects = (u8) reservedObjects;
		}
		else
		{
			m_ReservedObjects = 0;
		}
	}
	else
	{
		m_ReservedPeds = 0;
		m_ReservedVehicles = 0;
		m_ReservedObjects = 0;
	}
}

u32 CGameScriptHandlerMgr::CRemoteScriptInfo::GetCurrentTime() const
{
	return fwTimer::GetSystemTimeInMilliseconds();
}

scriptIdBase& CGameScriptHandlerMgr::GetScriptId(const scrThread& scriptThread) const
{
	static CGameScriptId scrId;

	scrId = CGameScriptId(scriptThread);

	return scrId;
}

scriptHandler* CGameScriptHandlerMgr::CreateScriptHandler(scrThread& scriptThread)
{
	return rage_new CGameScriptHandler(const_cast<scrThread&>(scriptThread));
}

void CGameScriptHandlerMgr::Init()
{
	if (scriptVerify(!m_IsInitialised))
	{
		CGameScriptHandler::InitPool( MEMBUCKET_SYSTEM );
		CGameScriptHandlerNetwork::InitPool( MEMBUCKET_SYSTEM );
		CGameScriptHandlerNetComponent::InitPool( MEMBUCKET_SYSTEM );
		fwScriptGuid::InitPool( MEMBUCKET_SYSTEM );		
		CScriptEntityExtension::InitPool( MEMBUCKET_SYSTEM );	
#if __DEV
		CScriptEntityExtension::GetPool()->RegisterPoolCallback(CScriptEntityExtension::PoolFullCallback);
#endif // __DEV

		CGameScriptResource::InitPool( MEMBUCKET_SYSTEM );

		scriptHandlerMgr::Init();
	}

#if __SCRIPT_MEM_CALC
	m_ArrayOfModelsUsedByMovieMeshes.Reset();
	m_MapOfMoviesUsedByMovieMeshes.Reset();
	m_MapOfRenderTargetsUsedByMovieMeshes.Reset();
	m_bTrackMemoryForMovieMeshes = false;

	m_TotalScriptVirtualMemory = 0;
	m_TotalScriptPhysicalMemory = 0;
	m_TotalScriptVirtualMemoryPeak = 0;
	m_TotalScriptPhysicalMemoryPeak = 0;
#endif	//	__SCRIPT_MEM_CALC

	scriptHandlerNetComponent::ms_useNoHostFix = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("USE_NO_HOST_FIX", 0x318526f9), scriptHandlerNetComponent::ms_useNoHostFix);
}

void CGameScriptHandlerMgr::NetworkInit()
{
	if (gnetVerify(!m_IsNetworkInitialised))
	{
		m_remoteActiveScripts.Reset();
		m_remoteTerminatedScripts.Reset();

		scriptHandlerMgr::NetworkInit();
	}

#if __SCRIPT_MEM_CALC
	m_TotalScriptVirtualMemoryPeak = 0;
	m_TotalScriptPhysicalMemoryPeak = 0;
#endif	//	__SCRIPT_MEM_CALC
}

void CGameScriptHandlerMgr::NetworkShutdown()
{
	scriptHandlerMgr::NetworkShutdown();

#if __SCRIPT_MEM_CALC
	m_TotalScriptVirtualMemoryPeak = 0;
	m_TotalScriptPhysicalMemoryPeak = 0;
#endif	//	__SCRIPT_MEM_CALC
}

void CGameScriptHandlerMgr::Shutdown()
{
	if (m_IsInitialised)
	{
		scriptHandlerMgr::Shutdown();

		CGameScriptResource::ShutdownPool();
		fwScriptGuid::ShutdownPool();
		CScriptEntityExtension::ShutdownPool();
		CGameScriptHandlerNetComponent::ShutdownPool();
		CGameScriptHandlerNetwork::ShutdownPool();
		CGameScriptHandler::ShutdownPool();
	}
}

void CGameScriptHandlerMgr::Update()
{
	scriptHandlerMgr::Update();
}

void CGameScriptHandlerMgr::NetworkUpdate()
{
	scriptHandlerMgr::NetworkUpdate();

	u32 i=0; 

	while (i < m_remoteActiveScripts.GetCount())
	{
		if (m_remoteActiveScripts[i].IsTerminated())
		{
			MoveActiveScriptToTerminatedQueue(i);
		}
		else
		{
			i++;
		}
	}
}

void CGameScriptHandlerMgr::PlayerHasLeft(const netPlayer& player)
{
	scriptHandlerMgr::PlayerHasLeft(player);

   // remove the player from the participant flags in all active scripts
	for (u32 i=0; i<m_remoteActiveScripts.GetCount(); i++)
	{
		CRemoteScriptInfo* pInfo = &m_remoteActiveScripts[i];

        if (pInfo->IsPlayerAParticipant(player.GetPhysicalPlayerIndex()))
		{
			pInfo->RemoveParticipant(player.GetPhysicalPlayerIndex());
		}

		if (pInfo->GetHost() == &player)
		{
			pInfo->SetHost(NULL);
		}
    }
}

#if __BANK
void Bank_ForceResend()
{
	CGameScriptHandlerMgr* pMgr = &CTheScripts::GetScriptHandlerMgr();

	const CGameScriptHandler* handler = pMgr->GetScriptHandlerFromComboName(pMgr->ms_Handlers[pMgr->ms_HandlerComboIndex]);

	if (handler && handler->GetNetworkComponent())
	{
		handler->GetNetworkComponent()->ForceResendOfAllBroadcastData();
	}
}

void Bank_HostRequest()
{
	CGameScriptHandlerMgr* pMgr = &CTheScripts::GetScriptHandlerMgr();

	const CGameScriptHandler* handler = pMgr->GetScriptHandlerFromComboName(pMgr->ms_Handlers[pMgr->ms_HandlerComboIndex]);

	if (handler && handler->GetNetworkComponent())
	{
		handler->GetNetworkComponent()->RequestToBeHost();
	}
}

void CGameScriptHandlerMgr::InitWidgets(bkBank* pBank)
{

	if (scriptVerify(pBank))
	{
		ms_Handlers[0] = "-- none --";
		ms_NumHandlers = 1;

		pBank->PushGroup("Script Handlers",false);
			ms_pHandlerCombo = pBank->AddCombo("Active Handlers", &ms_HandlerComboIndex, ms_NumHandlers, (const char **)ms_Handlers);
			pBank->AddToggle("Display handler info", &ms_DisplayHandlerInfo);
			pBank->AddButton("Resend all broadcast data", Bank_ForceResend);
			pBank->AddButton("Request to be host", Bank_HostRequest);
			pBank->AddToggle("Display all MP handlers", &ms_DisplayAllMPHandlers);
		pBank->PopGroup();
	}
}

//	CGameScriptHandlerMgr::ShutdownWidgets
//	Since ms_pHandlerCombo is created within the script bank, make sure that we remove the combo and set ms_pHandlerCombo to NULL
//			before the script bank is deleted so that ms_pHandlerCombo doesn't have an invalid address
void CGameScriptHandlerMgr::ShutdownWidgets(bkBank* pBank)
{
	if (scriptVerify(pBank))
	{
		pBank->Remove(*ms_pHandlerCombo);
		ms_pHandlerCombo = NULL;

		ms_HandlerComboIndex = 0;
		ms_DisplayHandlerInfo = false;
		ms_DisplayAllMPHandlers = false;
	}
}
#endif // __BANK

void CGameScriptHandlerMgr::RegisterScript(scrThread& scriptThread)
{
	scriptHandlerMgr::RegisterScript(scriptThread);

#if __BANK
	UpdateScriptHandlerCombo();
#endif
}

void CGameScriptHandlerMgr::UnregisterScript(scrThread& scriptThread)
{
	GtaThread& gtaThread = static_cast<GtaThread&>(scriptThread);

	CGameScriptHandler*	handler = gtaThread.m_Handler;

	if (AssertVerify(handler) && handler->Terminate())
	{
		DeleteHandlerWithHash(handler->GetScriptId().GetScriptIdHash());
	}

	gtaThread.m_Handler = NULL;

#if __BANK
	UpdateScriptHandlerCombo();
#endif
}

bool CGameScriptHandlerMgr::IsScriptValidForNetworking(atHashValue scriptName)
{
#if !__FINAL
	if (PARAM_dontCrashOnScriptValidation.Get())
	{
		return true;
	}
#endif // !__FINAL

	for (u32 scriptHash = 0; scriptHash < NUM_VALID_NETWORK_SCRIPTS; scriptHash++)
	{
		if (ms_validNetworkScriptHashes[scriptHash] == scriptName)
		{
			return true;
		}
	}

	return false;
}

void CGameScriptHandlerMgr::SetScriptAsANetworkScript(GtaThread& scriptThread, int instanceId)
{
	CGameScriptId& scriptId = static_cast<CGameScriptId&>(GetScriptId(scriptThread));

	atHashValue hash = scriptId.GetScriptIdHash();

	netLoggingInterface *log = CTheScripts::GetScriptHandlerMgr().GetLog();

	if (log)
	{
		CGameScriptId logId = scriptId;
		logId.SetInstanceId(instanceId);
		NetworkLogUtils::WriteLogEvent(*log, "SET_SCRIPT_AS_NETWORK_SCRIPT", "%s", logId.GetScriptName());
	}

	// find network handler for this thread, destroy it and replace with a network handler
	scriptHandler** ppHandler = m_scriptHandlers.Access(hash);

	if (ppHandler)
	{
		CGameScriptHandler* handler = static_cast<CGameScriptHandler*>(*ppHandler);

		scriptAssertf(handler->GetNumRegisteredNetworkedObjects() == 0, "CGameScriptHandlerMgr::SetScriptAsANetworkScript : Script %s has registered networked objects", handler->GetScriptName());

		if (scriptVerifyf(!handler->RequiresANetworkComponent(), "CGameScriptHandlerMgr::SetScriptAsANetworkScript : Script %s has already been set as a network script", handler->GetScriptName()))
		{
			if (!IsScriptValidForNetworking(scriptId.GetScriptNameHash()))
			{
#if !__FINAL
				NETWORK_QUITF(0, "CGameScriptHandlerMgr::SetScriptAsANetworkScript : Cannot network script %s - it has no entry in the valid network scripts allowlist", handler->GetScriptName());
#endif
				return;
			}

			if (CGameScriptHandlerNetwork::GetPool()->GetNoOfFreeSpaces() == 0)
			{
				Displayf("** TOO MANY NETWORKED SCRIPTS RUNNING!! **\n");
				Displayf("Active networked scripts:\n");

				scriptHandlerMap::Iterator entry = m_scriptHandlers.CreateIterator();

				for (entry.Start(); !entry.AtEnd(); entry.Next())
				{
					scriptHandler* handler = entry.GetData(); 

					if (handler && handler->GetNetworkComponent())
					{
						Displayf("%s\n", handler->GetLogName());
					}
				}

				CNetwork::FlushAllLogFiles(true);

				NETWORK_QUITF(0, "CGameScriptHandlerMgr::SetScriptAsANetworkScript : Cannot network this script - too many networked scripts running (max %d)", MAX_NUM_ACTIVE_NETWORK_SCRIPTS);
				return;
			}

			scriptHandlerObject* aUnnetworkedObjects[CGameScriptHandler::MAX_UNNETWORKED_OBJECTS];
			u32 numUnnetworkedObjects = 0;

			handler->DetachAllUnnetworkedObjects(aUnnetworkedObjects, numUnnetworkedObjects);

			DeleteHandlerWithHash(hash);

			scriptThread.bSafeForNetworkGame = true;
			scriptThread.InstanceId = instanceId;

			if (instanceId != -1)
			{
#if __ASSERT
				scriptHandler* pHandler = GetScriptHandler(GetScriptId(scriptThread));

				// we can't have 2 network script handlers with the same id
				NETWORK_QUITF(!pHandler, "SetScriptAsANetworkScript - a network script already exists with this instance id (%s)", pHandler->GetLogName());
#endif
			}

			CGameScriptHandlerNetwork* netHandler = rage_new CGameScriptHandlerNetwork(scriptThread);	

#if !__NO_OUTPUT
			scriptHandler** ppHandler = m_scriptHandlers.Access(netHandler->GetScriptId().GetScriptIdHash());
			if (ppHandler)
			{
				char logNameOfNewHandler[200];
				safecpy(logNameOfNewHandler, netHandler->GetLogName(), NELEM(logNameOfNewHandler));	//	CGameScriptId::GetLogName() fills in a static string internally so I need to make a copy of one of the LogName's to display in the assert below

#if __ASSERT
				CGameScriptHandler *pGameScriptHandler = static_cast<CGameScriptHandler*>(*ppHandler);

				Assertf(false, "CGameScriptHandlerMgr::SetScriptAsANetworkScript - about to add %s(%p) script with hash %u. %s(%p) already exists in the map with this hash",
					logNameOfNewHandler, netHandler, netHandler->GetScriptId().GetScriptIdHash().GetHash(),
					pGameScriptHandler->GetLogName(), pGameScriptHandler);

#endif
			}
#endif	//	!__NO_OUTPUT
			
			m_scriptHandlers.Insert(netHandler->GetScriptId().GetScriptIdHash(), netHandler);

			if (m_IsNetworkInitialised)
			{
				netHandler->CreateNetworkComponent();
			}
#if __BANK
			UpdateScriptHandlerCombo();
#endif
			for (u32 i=0; i<numUnnetworkedObjects; i++)
			{
				if (instanceId != -1)
				{
					static_cast<CGameScriptId&>(aUnnetworkedObjects[i]->GetScriptInfo()->GetScriptId()).SetInstanceId(instanceId);
				}

				netHandler->RegisterExistingScriptObject(*aUnnetworkedObjects[i]);
			}
		}
	}
	else
	{
		scriptAssertf(0, "CGameScriptHandlerMgr::SetScriptAsANetworkScript : The script handler for %s was not found", scriptId.GetLogName());
	}
}


bool CGameScriptHandlerMgr::TryToSetScriptAsANetworkScript(GtaThread& scriptThread, int instanceId)
{
	CGameScriptId& singlePlayerScriptId = static_cast<CGameScriptId&>(GetScriptId(scriptThread));

	atHashValue singlePlayerHash = singlePlayerScriptId.GetScriptIdHash();

	// find existing handler for this thread and attempt to replace it with a network handler
	scriptHandler** ppHandler = m_scriptHandlers.Access(singlePlayerHash);

	if (ppHandler)
	{
		CGameScriptHandler* pSinglePlayerHandler = static_cast<CGameScriptHandler*>(*ppHandler);

		scriptAssertf(pSinglePlayerHandler->GetNumRegisteredObjects() == 0, "CGameScriptHandlerMgr::TryToSetScriptAsANetworkScript : Script %s has registered objects", pSinglePlayerHandler->GetScriptName());

		if (scriptVerifyf(!pSinglePlayerHandler->RequiresANetworkComponent(), "CGameScriptHandlerMgr::TryToSetScriptAsANetworkScript : Script %s has already been set as a network script", pSinglePlayerHandler->GetScriptName()))
		{
			if (CGameScriptHandlerNetwork::GetPool()->GetNoOfFreeSpaces() == 0)
			{
				Displayf("** TOO MANY NETWORKED SCRIPTS RUNNING!! **\n");
				Displayf("Active networked scripts:\n");

				scriptHandlerMap::Iterator entry = m_scriptHandlers.CreateIterator();

				for (entry.Start(); !entry.AtEnd(); entry.Next())
				{
					scriptHandler* handler = entry.GetData(); 

					if (handler && handler->GetNetworkComponent())
					{
						Displayf("%s\n", handler->GetLogName());
					}
				}

				CNetwork::FlushAllLogFiles(true);

				FatalAssertf(0, "CGameScriptHandlerMgr::TryToSetScriptAsANetworkScript : Cannot network this script - too many networked scripts running (max %d)", MAX_NUM_ACTIVE_NETWORK_SCRIPTS);
				return false;
			}

			if (!m_scriptHandlers.Delete(singlePlayerHash))
			{
				FatalAssertf(0, "CGameScriptHandlerMgr::TryToSetScriptAsANetworkScript : failed to delete handler %s at key %u (%p)\n", pSinglePlayerHandler->GetLogName(), singlePlayerHash.GetHash(), pSinglePlayerHandler);
				return false;
			}

			bool bFailedToCreateNetworkHandler = false;
			CGameScriptHandlerNetwork* pNetHandler = NULL;

			bool singlePlayerSafeForNetworkGame = scriptThread.bSafeForNetworkGame;
			int singlePlayerInstanceId = scriptThread.InstanceId;

			scriptThread.bSafeForNetworkGame = true;

			if (instanceId != -1)
			{
				scriptThread.InstanceId = instanceId;

				scriptHandler* pHandler = GetScriptHandler(GetScriptId(scriptThread));

				// we can't have 2 network script handlers with the same id
				if (pHandler)
				{
					Displayf("CGameScriptHandlerMgr::TryToSetScriptAsANetworkScript - a network script already exists with this instance id (%s)", pHandler->GetLogName());
					bFailedToCreateNetworkHandler = true;
				}
			}

			if (!bFailedToCreateNetworkHandler)
			{
				pNetHandler = rage_new CGameScriptHandlerNetwork(scriptThread);

				scriptHandler** ppHandler = m_scriptHandlers.Access(pNetHandler->GetScriptId().GetScriptIdHash());
				if (ppHandler)
				{
#if !__NO_OUTPUT
					char logNameOfNewHandler[200];
					safecpy(logNameOfNewHandler, pNetHandler->GetLogName(), NELEM(logNameOfNewHandler));	//	CGameScriptId::GetLogName() fills in a static string internally so I need to make a copy of one of the LogName's to display in the assert below

					CGameScriptHandler *pGameScriptHandler = static_cast<CGameScriptHandler*>(*ppHandler);

					Displayf("CGameScriptHandlerMgr::TryToSetScriptAsANetworkScript - about to add %s(%p) script with hash %u. %s(%p) already exists in the map with this hash",
						logNameOfNewHandler, pNetHandler, pNetHandler->GetScriptId().GetScriptIdHash().GetHash(),
						pGameScriptHandler->GetLogName(), pGameScriptHandler);
#endif	//	!__NO_OUTPUT

					bFailedToCreateNetworkHandler = true;
				}
			}

			if (bFailedToCreateNetworkHandler)
			{
				if (pNetHandler)
				{	//	I don't need to Delete pNetHandler from m_scriptHandlers because it hasn't been Inserted at this stage
					m_DeletingHandlers = true;

					pNetHandler->Shutdown();
					delete pNetHandler;
					pNetHandler = NULL;

					m_DeletingHandlers = false;
				}

				// deleting pNetHandler will have cleared scriptThread.m_Handler (in ~CGameScriptHandler()) so make sure it points to pSinglePlayerHandler
				scriptThread.m_Handler = pSinglePlayerHandler;

				//	Put the original single player handler back to the way it was
				scriptThread.bSafeForNetworkGame = singlePlayerSafeForNetworkGame;
				scriptThread.InstanceId = singlePlayerInstanceId;

				m_scriptHandlers.Insert(singlePlayerHash, pSinglePlayerHandler);
			}
			else
			{
				if (pSinglePlayerHandler)
				{	//	We've already Deleted pSinglePlayerHandler from m_scriptHandlers so now we just need to delete pSinglePlayerHandler itself
					m_DeletingHandlers = true;

					pSinglePlayerHandler->Shutdown();
					delete pSinglePlayerHandler;
					pSinglePlayerHandler = NULL;

					m_DeletingHandlers = false;
				}

				// deleting pSinglePlayerHandler will have cleared scriptThread.m_Handler (in ~CGameScriptHandler()) so make sure it points to pNetHandler
				scriptThread.m_Handler = pNetHandler;

				m_scriptHandlers.Insert(pNetHandler->GetScriptId().GetScriptIdHash(), pNetHandler);

				if (m_IsNetworkInitialised)
				{
					pNetHandler->CreateNetworkComponent();
				}
			}
#if __BANK
			UpdateScriptHandlerCombo();
#endif

			return !bFailedToCreateNetworkHandler;
		}
	}
	else
	{
		scriptAssertf(0, "CGameScriptHandlerMgr::TryToSetScriptAsANetworkScript : The script handler for %s was not found", singlePlayerScriptId.GetLogName());
	}

	return false;
}



void CGameScriptHandlerMgr::SetScriptAsSafeToRunInMP(GtaThread& scriptThread)
{
	CGameScriptId& scriptId = static_cast<CGameScriptId&>(GetScriptId(scriptThread));

	atHashValue hash = scriptId.GetScriptIdHash();

	// find network handler for this thread and change its script id
	scriptHandler** ppHandler = m_scriptHandlers.Access(hash);

	if (ppHandler)
	{
		CGameScriptHandler* handler = static_cast<CGameScriptHandler*>(*ppHandler);

		if (!m_scriptHandlers.Delete(hash))
		{
			FatalAssertf(0, "CGameScriptHandlerMgr::SetScriptAsSafeToRunInMP - failed to remove handler %s from map", handler->GetLogName());
		}
	
		scriptThread.bSafeForNetworkGame = true;

		// recalculate the script id as the hash will now be altered
		static_cast<CGameScriptId&>(handler->GetScriptId()).InitGameScriptId(scriptThread);

#if !__NO_OUTPUT
		scriptHandler** ppHandler = m_scriptHandlers.Access(handler->GetScriptId().GetScriptIdHash());
		if (ppHandler)
		{
			char logNameOfNewHandler[200];
			safecpy(logNameOfNewHandler, handler->GetLogName(), NELEM(logNameOfNewHandler));	//	CGameScriptId::GetLogName() fills in a static string internally so I need to make a copy of one of the LogName's to display in the assert below

			CGameScriptHandler *pGameScriptHandler = static_cast<CGameScriptHandler*>(*ppHandler);

			Quitf("CGameScriptHandlerMgr::SetScriptAsSafeToRunInMP - about to add %s(%p) script with hash %u. %s(%p) already exists in the map with this hash",
				logNameOfNewHandler, handler, handler->GetScriptId().GetScriptIdHash().GetHash(),
				pGameScriptHandler->GetLogName(), pGameScriptHandler);
		}
#endif	//	!__NO_OUTPUT

		m_scriptHandlers.Insert(handler->GetScriptId().GetScriptIdHash(), handler);
	}
	else
	{
		scriptAssertf(0, "CGameScriptHandlerMgr::SetScriptAsSafeToRunInMP : The script handler for %s was not found", scriptId.GetLogName());
	}
}

u32 CGameScriptHandlerMgr::GetAllScriptHandlerEntities(CEntity* entityArray[], u32 arrayLen)
{
	u32 numEntities = 0;

	scriptHandlerMap::Iterator entry = m_scriptHandlers.CreateIterator();

	for (entry.Start(); !entry.AtEnd(); entry.Next())
	{
		CGameScriptHandler* handler = static_cast<CGameScriptHandler*>(entry.GetData()); 

		if (handler)
		{
			numEntities += handler->GetAllEntities(&entityArray[numEntities], arrayLen-numEntities);
		}
	}

	return numEntities;
}

void CGameScriptHandlerMgr::GetNumReservedScriptEntities(u32 &numPeds, u32 &numVehicles, u32 &numObjects) const
{
	numPeds = numVehicles = numObjects = 0;

	scriptHandlerMap::ConstIterator entry = m_scriptHandlers.CreateIterator();

	for (entry.Start(); !entry.AtEnd(); entry.Next())
	{
		CGameScriptHandler* handler = static_cast<CGameScriptHandler*>(entry.GetData()); 

		if (handler && handler->RequiresANetworkComponent())
		{
			const CGameScriptHandlerNetwork* netHandler = static_cast<const CGameScriptHandlerNetwork*>(handler);

			numPeds		+= netHandler->GetTotalNumReservedPeds();
			numVehicles += netHandler->GetTotalNumReservedVehicles();
			numObjects	+= netHandler->GetTotalNumReservedObjects();
		}
	}
}

void CGameScriptHandlerMgr::GetNumCreatedScriptEntities(u32 &numPeds, u32 &numVehicles, u32 &numObjects) const
{
	numPeds = numVehicles = numObjects = 0;

	scriptHandlerMap::ConstIterator entry = m_scriptHandlers.CreateIterator();

	for (entry.Start(); !entry.AtEnd(); entry.Next())
	{
		CGameScriptHandler* handler = static_cast<CGameScriptHandler*>(entry.GetData()); 

		if (handler && handler->RequiresANetworkComponent())
		{
			const CGameScriptHandlerNetwork* netHandler = static_cast<const CGameScriptHandlerNetwork*>(handler);

			numPeds		+= netHandler->GetNumCreatedPeds();
			numVehicles += netHandler->GetNumCreatedVehicles();
			numObjects	+= netHandler->GetNumCreatedObjects();
		}
	}
}

void CGameScriptHandlerMgr::GetNumRequiredScriptEntities(u32 &numPeds, u32 &numVehicles, u32 &numObjects, CGameScriptHandler* pHandlerToIgnore) const
{
	numPeds = numVehicles = numObjects = 0;

	scriptHandlerMap::ConstIterator entry = m_scriptHandlers.CreateIterator();

	for (entry.Start(); !entry.AtEnd(); entry.Next())
	{
		CGameScriptHandler* handler = static_cast<CGameScriptHandler*>(entry.GetData()); 

		if (handler && handler != pHandlerToIgnore && handler->GetNetworkComponent() && handler->GetNetworkComponent()->IsPlaying())
		{
			const CGameScriptHandlerNetwork* netHandler = static_cast<const CGameScriptHandlerNetwork*>(handler);

			u32 peds = 0;
			u32 vehs = 0;
			u32 objs = 0;

			netHandler->GetNumRequiredEntities(peds, vehs, objs);

			numPeds += peds;
			numVehicles += vehs;
			numObjects += objs;
		}
	}

	u32 remoteReservedPeds = 0;
	u32 remoteReservedVehs = 0;
	u32 remoteReservedObjs = 0;
	u32 remoteCreatedPeds = 0;
	u32 remoteCreatedVehs = 0;
	u32 remoteCreatedObjs = 0;

	GetRemoteScriptEntityReservations(remoteReservedPeds, remoteReservedVehs, remoteReservedObjs, remoteCreatedPeds, remoteCreatedVehs, remoteCreatedObjs);

	if (remoteReservedPeds > remoteCreatedPeds)
	{
		numPeds += remoteReservedPeds-remoteCreatedPeds;
	}

	if (remoteReservedVehs > remoteCreatedVehs)
	{
		numVehicles += remoteReservedVehs-remoteCreatedVehs;
	}

	if (remoteReservedObjs > remoteCreatedObjs)
	{
		numObjects += remoteReservedObjs-remoteCreatedObjs;
	}
}

//PURPOSE
//	Adds info on a new script running remotely but not locally
void CGameScriptHandlerMgr::AddRemoteScriptInfo(CRemoteScriptInfo& scriptInfo, const netPlayer* sendingPlayer)
{
	CGameScriptHandler* pHandler = SafeCast(CGameScriptHandler, GetScriptHandler(scriptInfo.GetScriptId()));
	
	if (gnetVerifyf(scriptInfo.GetScriptId().GetTimeStamp() != 0, "CGameScriptHandlerMgr::AddRemoteScriptInfo: can't add a remote script info with a timestamp of 0"))
	{
		if (sendingPlayer)
		{
			WriteRemoteScriptHeader(scriptInfo.GetScriptId(), "Got a remote script info update from %s. Participants: %u, Host token: %u. Reservations: peds(%d), vehicles(%d), objects(%d)", sendingPlayer->GetLogName(), scriptInfo.GetActiveParticipants(), scriptInfo.GetHostToken(), scriptInfo.GetReservedPeds(), scriptInfo.GetReservedVehicles(), scriptInfo.GetReservedObjects());
			
			// the sending player is the current host
			scriptInfo.SetHost(sendingPlayer);
			
			if (pHandler && !pHandler->IsTerminated())
			{
				WriteRemoteScriptHeader(scriptInfo.GetScriptId(), "Ignore: script is running locally");
				return;
			}
		}
		else
		{
			WriteRemoteScriptHeader(scriptInfo.GetScriptId(), "Locally update remote script info. Participants: %u, Host token: %u. Reservations: peds(%d), vehicles(%d), objects(%d)", scriptInfo.GetActiveParticipants(), scriptInfo.GetHostToken(), scriptInfo.GetReservedPeds(), scriptInfo.GetReservedVehicles(), scriptInfo.GetReservedObjects());
		}

		if (scriptInfo.GetActiveParticipants() == 0)
		{
			// this script has terminated, so remove from active queue and add to terminated queue
			for (u32 i=0; i<m_remoteActiveScripts.GetCount(); i++)
			{
				if (m_remoteActiveScripts[i].m_ScriptId == scriptInfo.m_ScriptId)
				{
					// if the host token is less than the one we currently have, then this info is from a previous host and is ignored.
					if (scriptInfo.m_HostToken < m_remoteActiveScripts[i].m_HostToken)
					{
						WriteRemoteScriptHeader(scriptInfo.GetScriptId(), "Host token out of date (most recent: %u). Ignore.", m_remoteActiveScripts[i].m_HostToken);
					}
					else
					{
						MoveActiveScriptToTerminatedQueue(i);
					}
					
					break;
				}
			}
		}
		else
		{
			bool bAddActiveScript = true;

			// make sure all players still exist (they may have left before we received this update)
			for (PhysicalPlayerIndex i=0; i<MAX_NUM_PHYSICAL_PLAYERS; i++)
			{
				if (scriptInfo.IsPlayerAParticipant(i) && !NetworkInterface::GetPhysicalPlayerFromIndex(i))
				{
					WriteRemoteScriptHeader(scriptInfo.GetScriptId(), "Removed a non-existent player %d from the remote script info update.", i);
					scriptInfo.RemoveParticipant(i);

					if (scriptInfo.GetActiveParticipants() == 0)
						return;
				}
			}

			// check to see if this script is on the terminated queue. This can happen when we are the host and terminate the script, and another player temporarily 
			// becomes the host and sends another remote script info update back to us before shutting down the script not long after. Once a script is on the terminated
			// queue it must stay there.
			for (u32 i=0; i<m_remoteTerminatedScripts.GetCount(); i++)
			{
				if (m_remoteTerminatedScripts[i].m_ScriptId == scriptInfo.m_ScriptId)
				{
					WriteRemoteScriptHeader(scriptInfo.GetScriptId(), "Remote script info is already on terminated queue. Ignore update.");
					return;
				}	
			}
			
			// see if we already have an info for this script in the active queue
			for (u32 i=0; i<m_remoteActiveScripts.GetCount(); i++)
			{
				if (m_remoteActiveScripts[i].m_ScriptId == scriptInfo.m_ScriptId)
				{
					// if the host token is less than the one we currently have, then this info is from a previous host and is ignored.
					if (scriptInfo.m_HostToken < m_remoteActiveScripts[i].m_HostToken)
					{
						WriteRemoteScriptHeader(scriptInfo.GetScriptId(), "Host token out of date (most recent: %u). Ignore.", m_remoteActiveScripts[i].m_HostToken);
					}
					else
					{
						WriteRemoteScriptHeader(scriptInfo.GetScriptId(), "Update existing remote script info");
					
						// update the existing entry with the latest info
						m_remoteActiveScripts[i].UpdateInfo(scriptInfo);
					}

					bAddActiveScript = false;
					break;
				}
			}

			if (bAddActiveScript && sendingPlayer)
			{
				// possibly handle the case where we have 2 identical scripts running with different timestamps
				bAddActiveScript = HandleTimestampConflict(scriptInfo);
			}

			if (bAddActiveScript)
			{
				// we don't want to fill up the queue for the active list.
				gnetAssertf(m_remoteActiveScripts.GetAvailable() > 0, "Ran out of active remote script infos");

				WriteRemoteScriptHeader(scriptInfo.GetScriptId(), "Add new active remote script info");

				// add a new info to the active queue
				if (!m_remoteActiveScripts.Push(scriptInfo))
				{
					Assertf(0, "The remote active script queue is full");
				}
			}
		}
	}
}

void CGameScriptHandlerMgr::AddPlayerToRemoteScript(const CGameScriptId& scriptId, const netPlayer& player)
{
	if (gnetVerifyf(scriptId.GetTimeStamp() != 0, "CGameScriptHandlerMgr::AddPlayerToRemoteScript: scriptId has a timestamp of 0"))
	{
		WriteRemoteScriptHeader(scriptId, "Add remote player %s (%d) to remote script", player.GetLogName(), player.GetPhysicalPlayerIndex());

		for (u32 i=0; i<m_remoteActiveScripts.GetCount(); i++)
		{
			if (m_remoteActiveScripts[i].m_ScriptId == scriptId)
			{
				m_remoteActiveScripts[i].AddActiveParticipant(player.GetPhysicalPlayerIndex());
				WriteRemoteScriptHeader(scriptId, "Participants now = %d", m_remoteActiveScripts[i].GetActiveParticipants());			
				return;
			}
		}

		for (u32 i=0; i<m_remoteTerminatedScripts.GetCount(); i++)
		{
			if (m_remoteTerminatedScripts[i].m_ScriptId == scriptId)
			{
				gnetAssertf(0, "Trying to add a participant to a terminated remote script");
				return;
			}	
		}

		WriteRemoteScriptHeader(scriptId, "Active remote script not found. Ignore");
	}
}

void CGameScriptHandlerMgr::RemovePlayerFromRemoteScript(const CGameScriptId& scriptId, const netPlayer& player, bool bCalledFromHandler)
{
	if (gnetVerifyf(scriptId.GetTimeStamp() != 0, "CGameScriptHandlerMgr::RemovePlayerFromRemoteScript: scriptId %s has a timestamp of 0", scriptId.GetLogName()))
	{
		WriteRemoteScriptHeader(scriptId, "Remove remote player %s (%d) from remote script", player.GetLogName(), player.GetPhysicalPlayerIndex());

		if (!bCalledFromHandler)
		{
			scriptHandler* pHandler = GetScriptHandler(scriptId);

			if (pHandler && !pHandler->IsTerminated())
			{
				WriteRemoteScriptHeader(scriptId, "Ignore: script is running locally");
				return;
			}
		}

		for (u32 i=0; i<m_remoteActiveScripts.GetCount(); i++)
		{
			if (m_remoteActiveScripts[i].m_ScriptId == scriptId)
			{
				m_remoteActiveScripts[i].RemoveParticipant(player.GetPhysicalPlayerIndex());

				WriteRemoteScriptHeader(scriptId, "Participants now = %d", m_remoteActiveScripts[i].GetActiveParticipants());			

				if (m_remoteActiveScripts[i].GetActiveParticipants() == 0)
				{
					MoveActiveScriptToTerminatedQueue(i);
				}

				return;
			}
		}

#if ENABLE_NETWORK_LOGGING
		for (u32 i=0; i<m_remoteTerminatedScripts.GetCount(); i++)
		{
			if (m_remoteTerminatedScripts[i].m_ScriptId == scriptId)
			{
				WriteRemoteScriptHeader(scriptId, "Remote script has already been terminated. Ignore.");
				return;
			}	
		}

		WriteRemoteScriptHeader(scriptId, "No remote script found. Ignore");
#endif
	}
}

void CGameScriptHandlerMgr::TerminateActiveScriptInfo(const CGameScriptId& scriptId)
{
	CGameScriptId idNoTimestamp = scriptId;

	// reset the timestamp so that comparisons will find matching scripts
	idNoTimestamp.ResetTimestamp();

	for (u32 i=0; i<m_remoteActiveScripts.GetCount(); i++)
	{
		if (m_remoteActiveScripts[i].GetScriptId() == idNoTimestamp && 
			m_remoteActiveScripts[i].m_ScriptId.GetTimeStamp() != scriptId.GetTimeStamp())
		{
			WriteRemoteScriptHeader(m_remoteActiveScripts[i].GetScriptId(), "Terminate existing active id");
			MoveActiveScriptToTerminatedQueue(i);
			break;
		}
	}
}

void CGameScriptHandlerMgr::SetNewHostOfRemoteScript(const CGameScriptId& scriptId, const netPlayer& newHost, HostToken newHostToken, PlayerFlags participantFlags)
{
	if (gnetVerifyf(scriptId.GetTimeStamp() != 0, "CGameScriptHandlerMgr::SetNewHostOfRemoteScript: scriptId has a timestamp of 0"))
	{
		WriteRemoteScriptHeader(scriptId, "Setting new host %s (%d) of remote script. Host token: %u", newHost.GetLogName(), newHost.GetPhysicalPlayerIndex(), newHostToken);

		for (u32 i=0; i<m_remoteActiveScripts.GetCount(); i++)
		{
			if (m_remoteActiveScripts[i].m_ScriptId == scriptId)
			{
				m_remoteActiveScripts[i].SetHostToken(newHostToken);
				m_remoteActiveScripts[i].SetParticipants(participantFlags);
				return;
			}
		}

		WriteRemoteScriptHeader(scriptId, "Active remote script not found. Ignore");
	}
}

//PURPOSE
//	Returns the closest participant of the given remote script
CNetGamePlayer* CGameScriptHandlerMgr::GetClosestParticipantOfRemoteScript(const CRemoteScriptInfo& remoteScriptInfo)
{
	CNetGamePlayer* pClosestPlayer = NULL;
	float closestDist = 0.0f;
	Vector3 localPlayerPos = VEC3V_TO_VECTOR3(CGameWorld::FindLocalPlayer()->GetTransform().GetPosition());

	PlayerFlags	participantFlags = remoteScriptInfo.GetActiveParticipants();

	for (u32 i=0; i<MAX_NUM_PHYSICAL_PLAYERS; i++)
	{
		if (participantFlags & (1<<i))
		{
			CNetGamePlayer* pPlayer = static_cast<CNetGamePlayer*>(NetworkInterface::GetPlayerMgr().GetPhysicalPlayerFromIndex((PhysicalPlayerIndex)i));

			if (pPlayer->GetPlayerPed() && !pPlayer->IsMyPlayer())
			{
				Vector3 playerPos = VEC3V_TO_VECTOR3(pPlayer->GetPlayerPed()->GetTransform().GetPosition());
				Vector3 diff = playerPos - localPlayerPos;

				float playerDist = diff.XYMag2();

				if (!pClosestPlayer || playerDist < closestDist)
				{
					pClosestPlayer = pPlayer;
					closestDist = playerDist;
				}
			}
		}
	}

	return pClosestPlayer;
}

//PURPOSE
//	Gets info on a new script running remotely but not locally
CGameScriptHandlerMgr::CRemoteScriptInfo* CGameScriptHandlerMgr::GetRemoteScriptInfo(const CGameScriptId& scriptId, bool bActiveOnly) 
{
	for (u32 i=0; i<m_remoteActiveScripts.GetCount(); i++)
	{
		if (m_remoteActiveScripts[i].m_ScriptId == scriptId)
		{
			if (!bActiveOnly || m_remoteActiveScripts[i].GetActiveParticipants() > 0) 
				return &m_remoteActiveScripts[i];
			else
				return NULL;
		}
	}

	if (!bActiveOnly)
	{
		for (u32 i=0; i<m_remoteTerminatedScripts.GetCount(); i++)
		{
			if (m_remoteTerminatedScripts[i].m_ScriptId == scriptId)
			{
				return &m_remoteTerminatedScripts[i];
			}
		}
	}

	return NULL;
}

const CGameScriptHandlerMgr::CRemoteScriptInfo* CGameScriptHandlerMgr::GetRemoteScriptInfo(const CGameScriptId& scriptId, bool bActiveOnly) const
{
	return const_cast<CGameScriptHandlerMgr*>(this)->GetRemoteScriptInfo(scriptId, bActiveOnly);
}


// returns true if the given script is running, either locally or remotely
bool CGameScriptHandlerMgr::IsScriptActive(const CGameScriptId& scriptId, bool bMatchOnNameOnly)
{
	bool bScriptActive = false;

	if (GetScriptHandler(scriptId))
	{
		bScriptActive = true;
	}
	else
	{
		for (u32 i=0; i<m_remoteActiveScripts.GetCount(); i++)
		{
			if (m_remoteActiveScripts[i].GetActiveParticipants() > 0)
			{
				if (bMatchOnNameOnly)
				{
					// only match using the script name
					if (m_remoteActiveScripts[i].m_ScriptId.EqualivalentScriptName(scriptId))
					{
						bScriptActive = true;
						break;
					}
				}
				else if (m_remoteActiveScripts[i].m_ScriptId == scriptId)
				{
					bScriptActive = true;
					break;
				}
			}
		}
	}

	return bScriptActive;
}

// returns true if the given script has been terminated
bool CGameScriptHandlerMgr::IsScriptTerminated(const CGameScriptId& scriptId)
{
	if (GetScriptHandler(scriptId) && !GetScriptHandler(scriptId)->IsTerminated())
	{
		return false;
	}

	for (u32 i=0; i<m_remoteTerminatedScripts.GetCount(); i++)
	{
		if (m_remoteTerminatedScripts[i].m_ScriptId == scriptId)
		{
			return true;
		}
	}

	return false;
}

// returns true if the given player is a participant in the given script, either local or remote
bool CGameScriptHandlerMgr::IsPlayerAParticipant(const CGameScriptId& scriptId, const netPlayer& player)
{
	scriptHandler* pHandler = GetScriptHandler(scriptId);

	if (pHandler && pHandler->GetNetworkComponent() && pHandler->GetNetworkComponent()->IsPlayerAParticipant(player))
	{
		return true;
	}

	for (u32 i=0; i<m_remoteActiveScripts.GetCount(); i++)
	{
		if (m_remoteActiveScripts[i].m_ScriptId == scriptId && m_remoteActiveScripts[i].IsPlayerAParticipant(player.GetPhysicalPlayerIndex()))
		{
			return true;
		}
	}

	return false;
}

bool CGameScriptHandlerMgr::IsNetworkHost(const CGameScriptId& scriptId)
{
    bool isHost = false;

    scriptHandler *handler = GetScriptHandler(scriptId);
	scriptHandlerNetComponent* netComponent = handler ? handler->GetNetworkComponent() : NULL;

	if (netComponent && netComponent->GetState() == scriptHandlerNetComponent::NETSCRIPT_PLAYING)
	{
		const netPlayer* pHost = netComponent->GetHost();

		if (pHost && pHost->IsMyPlayer())
	    {
		    isHost = true;
		}
	}

    return isHost;
}

u32 CGameScriptHandlerMgr::GetNumParticipantsOfScript(const CGameScriptId& scriptId)
{
	u32 numParticipants = 0;

	scriptHandler* pHandler = GetScriptHandler(scriptId);

	if (pHandler && pHandler->GetNetworkComponent())
	{
		numParticipants = pHandler->GetNetworkComponent()->GetNumParticipants();
	}
	else
	{
		u32 i = 0;

		for (i=0; i<m_remoteActiveScripts.GetCount(); i++)
		{
			if (m_remoteActiveScripts[i].m_ScriptId == scriptId)
			{
				numParticipants = m_remoteActiveScripts[i].GetNumParticipants();
				break;
			}
		}	

		Assertf(i<m_remoteActiveScripts.GetCount(), "GetNumParticipantsOfScript: Remote script not running");
	}

	return numParticipants;
}

void CGameScriptHandlerMgr::RecalculateScriptId(CGameScriptHandler& handler)
{
	if (!m_scriptHandlers.Delete(handler.GetScriptId().GetScriptIdHash()))
	{
		FatalAssertf(0, "CGameScriptHandlerMgr::RecalculateScriptId - failed to remove handler %s from map", handler.GetLogName());
	}
#if __ASSERT
	if (NetworkInterface::IsGameInProgress())
	{
		CGameScriptId scriptId(*handler.GetThread());
		Assert(!GetScriptHandler(scriptId));
	}
#endif

	static_cast<CGameScriptId&>(handler.GetScriptId()).InitGameScriptId(*handler.GetThread());

#if !__NO_OUTPUT
	scriptHandler** ppHandler = m_scriptHandlers.Access(handler.GetScriptId().GetScriptIdHash());
	if (ppHandler)
	{
		char logNameOfNewHandler[200];
		safecpy(logNameOfNewHandler, handler.GetLogName(), NELEM(logNameOfNewHandler));	//	CGameScriptId::GetLogName() fills in a static string internally so I need to make a copy of one of the LogName's to display in the assert below

		CGameScriptHandler *pGameScriptHandler = static_cast<CGameScriptHandler*>(*ppHandler);

		Quitf("CGameScriptHandlerMgr::RecalculateScriptId - about to add %s(%p) script with hash %u. %s(%p) already exists in the map with this hash",
			logNameOfNewHandler, &handler, handler.GetScriptId().GetScriptIdHash().GetHash(),
			pGameScriptHandler->GetLogName(), pGameScriptHandler);
	}
#endif	//	!__NO_OUTPUT

	m_scriptHandlers.Insert(handler.GetScriptId().GetScriptIdHash(), &handler);
}

void CGameScriptHandlerMgr::SetNotInMPCutscene()
{
	scriptHandlerMap::Iterator entry = m_scriptHandlers.CreateIterator();

	for (entry.Start(); !entry.AtEnd(); entry.Next())
	{
		CGameScriptHandler* handler = static_cast<CGameScriptHandler*>(entry.GetData()); 

		if (handler && handler->RequiresANetworkComponent())
		{
			CGameScriptHandlerNetwork* netHandler = static_cast<CGameScriptHandlerNetwork*>(handler);

			netHandler->SetInMPCutscene(false);
		}
	}
}

void CGameScriptHandlerMgr::AddToReservationCount(CNetObjGame& scriptEntityObj)
{
	if (!scriptEntityObj.IsLocalFlagSet(CNetObjGame::LOCALFLAG_COUNTED_BY_RESERVATIONS))
	{
		CEntity* pEntity = scriptEntityObj.GetEntity();

		if (pEntity)
		{
			bool bPed = pEntity->GetIsTypePed();
			bool bVeh = pEntity->GetIsTypeVehicle();
			bool bObj = pEntity->GetIsTypeObject() && scriptEntityObj.GetObjectType() == NET_OBJ_TYPE_OBJECT; // ignore doors and pickups
			
			if (bPed ||	bVeh || bObj)
			{
				CScriptEntityExtension* pExtension = pEntity->GetExtension<CScriptEntityExtension>();

				const CGameScriptId* pScriptId = NULL;

				if (pExtension)
				{
					pScriptId = static_cast<const CGameScriptId*>(&pExtension->GetScriptInfo()->GetScriptId());
				}

				if (pScriptId)
				{
					for (u32 i=0; i<m_remoteActiveScripts.GetCount(); i++)
					{
						if (m_remoteActiveScripts[i].m_ScriptId == *pScriptId)
						{
							if (bPed)
							{
								m_remoteActiveScripts[i].AddCreatedPed();
							}
							else if (bVeh)
							{
								m_remoteActiveScripts[i].AddCreatedVehicle();
							}
							else if (bObj) 
							{
								m_remoteActiveScripts[i].AddCreatedObject();
							}

							scriptEntityObj.SetLocalFlag(CNetObjGame::LOCALFLAG_COUNTED_BY_RESERVATIONS, true);

							netLoggingInterface* log = CTheScripts::GetScriptHandlerMgr().GetLog();

							if (log)
							{
								NetworkLogUtils::WriteLogEvent(*log, "RESERVATION_ENTITY_ADDED", "%s      %s", pScriptId->GetLogName(), scriptEntityObj.GetLogName());
								LogCurrentReservations(*log);
							}

							break;
						}
					}	
				}			
			}
		}
	}
}

void CGameScriptHandlerMgr::RemoveFromReservationCount(CNetObjGame& scriptEntityObj)
{
	if (scriptEntityObj.IsLocalFlagSet(CNetObjGame::LOCALFLAG_COUNTED_BY_RESERVATIONS))
	{
		CEntity* pEntity = scriptEntityObj.GetEntity();

		if (gnetVerifyf(pEntity, "Failed to remove reservation count for %s. Gameobject is missing", scriptEntityObj.GetLogName()))
		{
			bool bPed = pEntity->GetIsTypePed();
			bool bVeh = pEntity->GetIsTypeVehicle();
			bool bObj = pEntity->GetIsTypeObject() && scriptEntityObj.GetObjectType() == NET_OBJ_TYPE_OBJECT; // ignore doors and pickups

			CScriptEntityExtension* pExtension = pEntity->GetExtension<CScriptEntityExtension>();

			const CGameScriptId* pScriptId = NULL;

			if (pExtension && pExtension->GetScriptInfo())
			{
				pScriptId = static_cast<const CGameScriptId*>(&pExtension->GetScriptInfo()->GetScriptId());
			}

			if (pScriptId)
			{
				for (u32 i=0; i<m_remoteActiveScripts.GetCount(); i++)
				{
					if (m_remoteActiveScripts[i].m_ScriptId == *pScriptId)
					{
						if (bPed)
						{
							m_remoteActiveScripts[i].RemoveCreatedPed();
						}
						else if (bVeh)
						{
							m_remoteActiveScripts[i].RemoveCreatedVehicle();
						}
						else if (bObj) 
						{
							m_remoteActiveScripts[i].RemoveCreatedObject();
						}

						scriptEntityObj.SetLocalFlag(CNetObjGame::LOCALFLAG_COUNTED_BY_RESERVATIONS, false);

						netLoggingInterface* log = CTheScripts::GetScriptHandlerMgr().GetLog();

						if (log)
						{
							NetworkLogUtils::WriteLogEvent(*log, "RESERVATION_ENTITY_REMOVED", "%s      %s", pScriptId->GetLogName(), scriptEntityObj.GetLogName());
							LogCurrentReservations(*log);
						}
						break;
					}
				}
			}
		}
	}
}

void CGameScriptHandlerMgr::GetRemoteScriptEntityReservations(u32& reservedPeds, u32& reservedVehicles, u32& reservedObjects, u32& createdPeds, u32& createdVehicles, u32& createdObjects, Vector3* pScopePosition) const
{
	reservedPeds = 0;
	reservedVehicles = 0;
	reservedObjects = 0;
	createdPeds = 0;
	createdVehicles = 0;
	createdObjects = 0;

	PlayerFlags playersNearby = 0;
	PlayerFlags playersNearbyInPedScope = 0;
	PlayerFlags playersNearbyInVehScope = 0;
	PlayerFlags playersNearbyInObjScope = 0;

	bool playersNearbyCalculated = false;

	for (u32 i=0; i<m_remoteActiveScripts.GetCount(); i++)
	{
		const CRemoteScriptInfo* pInfo = &m_remoteActiveScripts[i];

		if (!pInfo->GetRunningLocally())
		{
			u32 peds = 0;
			u32 vehs = 0;
			u32 objs = 0;

			pInfo->GetReservations(peds, vehs, objs);

			if (peds != 0 || vehs != 0 || objs != 0)
			{
				if (!playersNearbyCalculated)
				{
					NetworkInterface::GetObjectManager().GetPlayersNearbyInScope(playersNearbyInPedScope, playersNearbyInVehScope, playersNearbyInObjScope, pScopePosition);

					playersNearby = playersNearbyInPedScope | playersNearbyInVehScope | playersNearbyInObjScope;

					playersNearbyCalculated = true;
				}

				if (playersNearby != 0)
				{
					bool bParticipantNearbyInPedScope = false;
					bool bParticipantNearbyInVehScope = false;
					bool bParticipantNearbyInObjScope = false;

					for (u32 p=0; p<MAX_NUM_PHYSICAL_PLAYERS; p++)
					{
						u32 playerIndex = (1<<p);

						if ((playersNearby & playerIndex) && (pInfo->GetActiveParticipants() & playerIndex))
						{
							Assert(playersNearbyCalculated);

							if (playersNearbyInPedScope & playerIndex)
							{
								bParticipantNearbyInPedScope = true;
							}

							if (playersNearbyInVehScope & playerIndex)
							{
								bParticipantNearbyInVehScope = true;
							}

							if (playersNearbyInObjScope & playerIndex)
							{
								bParticipantNearbyInObjScope = true;
							}
						}

						if (bParticipantNearbyInPedScope == (peds != 0) &&
							bParticipantNearbyInVehScope == (vehs != 0) &&
							bParticipantNearbyInObjScope == (objs != 0))
						{
							break;
						}
					}

					if (bParticipantNearbyInPedScope)
					{
						reservedPeds += peds;
						createdPeds += pInfo->GetCreatedPeds();
					}

					if (bParticipantNearbyInVehScope)
					{
						reservedVehicles += vehs;
						createdVehicles += pInfo->GetCreatedVehicles();
					}

					if (bParticipantNearbyInObjScope)
					{
						reservedObjects += objs;
						createdObjects += pInfo->GetCreatedObjects();
					}
				}
			}
		}
	}
}

#if ENABLE_NETWORK_LOGGING
void CGameScriptHandlerMgr::LogCurrentReservations(netLoggingInterface& log) const
{
	u32 reservedPeds = 0;
	u32 reservedVehs = 0;
	u32 reservedObjs = 0;

	bool bLogLocal = false;
	bool bLogRemote = false;

	scriptHandlerMap::ConstIterator entry = m_scriptHandlers.CreateIterator();

	for (entry.Start(); !entry.AtEnd(); entry.Next())
	{
		CGameScriptHandler* handler = static_cast<CGameScriptHandler*>(entry.GetData()); 

		if (handler && handler->RequiresANetworkComponent() && !handler->IsTerminated())
		{
			const CGameScriptHandlerNetwork* netHandler = static_cast<const CGameScriptHandlerNetwork*>(handler);

			reservedPeds = netHandler->GetTotalNumReservedPeds();
			reservedVehs = netHandler->GetTotalNumReservedVehicles();
			reservedObjs = netHandler->GetTotalNumReservedObjects();

			if (reservedPeds > 0 || reservedVehs > 0 || reservedObjs > 0)
			{
				if (!bLogLocal)
				{
					NetworkLogUtils::WriteLogEvent(log, "LOCAL_SCRIPT_RESERVATIONS", "");
					bLogLocal = true;
				}
				
				log.WriteDataValue(netHandler->GetScriptId().GetLogName(), "%d/%d peds, %d/%d vehicles, %d/%d objects", netHandler->GetNumCreatedPeds(), reservedPeds, netHandler->GetNumCreatedVehicles(), reservedVehs, netHandler->GetNumCreatedObjects(), reservedObjs);
			}
		}
	}

	PlayerFlags playersNearby = 0;
	PlayerFlags playersNearbyInPedScope = 0;
	PlayerFlags playersNearbyInVehScope = 0;
	PlayerFlags playersNearbyInObjScope = 0;

	bool playersNearbyCalculated = false;

	for (u32 i=0; i<m_remoteActiveScripts.GetCount(); i++)
	{
		const CRemoteScriptInfo* pInfo = &m_remoteActiveScripts[i];

		if (!pInfo->GetRunningLocally())
		{
			reservedPeds = 0;
			reservedVehs = 0;
			reservedObjs = 0;

			pInfo->GetReservations(reservedPeds, reservedVehs, reservedObjs);

			if (reservedPeds != 0 || reservedVehs != 0 || reservedObjs != 0)
			{
				if (!playersNearbyCalculated)
				{
					NetworkInterface::GetObjectManager().GetPlayersNearbyInScope(playersNearbyInPedScope, playersNearbyInVehScope, playersNearbyInObjScope);

					playersNearby = playersNearbyInPedScope | playersNearbyInVehScope | playersNearbyInObjScope;

					playersNearbyCalculated = true;
				}

				if (playersNearby != 0)
				{
					bool bParticipantNearbyInPedScope = false;
					bool bParticipantNearbyInVehScope = false;
					bool bParticipantNearbyInObjScope = false;

					for (u32 p=0; p<MAX_NUM_PHYSICAL_PLAYERS; p++)
					{
						u32 playerIndex = (1<<p);

						if ((playersNearby & playerIndex) && (pInfo->GetActiveParticipants() & playerIndex))
						{
							Assert(playersNearbyCalculated);

							if (playersNearbyInPedScope & playerIndex)
							{
								bParticipantNearbyInPedScope = true;
							}

							if (playersNearbyInVehScope & playerIndex)
							{
								bParticipantNearbyInVehScope = true;
							}

							if (playersNearbyInObjScope & playerIndex)
							{
								bParticipantNearbyInObjScope = true;
							}
						}
					}

					if (!bParticipantNearbyInPedScope)
					{
						reservedPeds = 0;
					}

					if (!bParticipantNearbyInVehScope)
					{
						reservedVehs = 0;
					}

					if (!bParticipantNearbyInObjScope)
					{
						reservedObjs = 0;
					}
				}
			}

			if (reservedPeds>0 || reservedVehs>0 || reservedObjs>0)
			{
				if (!bLogRemote)
				{
					NetworkLogUtils::WriteLogEvent(log, "REMOTE_SCRIPT_RESERVATIONS", "");
					bLogRemote = true;
				}

				log.WriteDataValue(m_remoteActiveScripts[i].GetScriptId().GetLogName(), "%d/%d peds, %d/%d vehicles, %d/%d objects", pInfo->GetCreatedPeds(), reservedPeds, pInfo->GetCreatedVehicles(), reservedVehs, pInfo->GetCreatedObjects(), reservedObjs);
			}
		}
	}	
}
#else
void CGameScriptHandlerMgr::LogCurrentReservations(netLoggingInterface&) const
{
}
#endif // ENABLE_NETWORK_LOGGING

#if __BANK

void CGameScriptHandlerMgr::DisplayScriptHandlerInfo() const
{
	if (ms_DisplayHandlerInfo)
	{
		const CGameScriptHandler* handler = GetScriptHandlerFromComboName(ms_Handlers[ms_HandlerComboIndex]);

		if (handler)
		{
			handler->DisplayScriptHandlerInfo();

			grcDebugDraw::AddDebugOutput("");
			grcDebugDraw::AddDebugOutput("");
		}
	}

	if (ms_DisplayAllMPHandlers && NetworkInterface::GetLocalPlayer()) 
	{
		grcDebugDraw::AddDebugOutput("");
		grcDebugDraw::AddDebugOutput("Local net array data: %u (Cached value: %u)", NetworkInterface::GetArrayManager().GetSizeOfLocallyArbitratedArrayData(), NetworkInterface::GetLocalPlayer()->GetSizeOfNetArrayData());
		grcDebugDraw::AddDebugOutput("");
		
		scriptHandlerMap::ConstIterator entry = m_scriptHandlers.CreateIterator();

		for (entry.Start(); !entry.AtEnd(); entry.Next())
		{
			scriptHandler* handler = entry.GetData(); 

			if (handler && handler->GetNetworkComponent())
			{
				char name[80];

				sprintf(name, "%-80s", handler->GetLogName());

				u32 size = handler->GetNetworkComponent()->GetSizeOfHostBroadcastData();

				if (handler->GetNetworkComponent()->IsHostMigrating())
				{
					grcDebugDraw::AddDebugOutput("%s (Size: %u. Host: ** migrating to %s **)", name, size, handler->GetNetworkComponent()->GetPendingHost()->IsLocal() ? "us" : handler->GetNetworkComponent()->GetPendingHost()->GetLogName());
				}
				else if (!handler->GetNetworkComponent()->GetHost())
				{
					grcDebugDraw::AddDebugOutput("%s (Size: %u. Host: --none--)", name, size);
				}
				else
				{
					if (handler->GetNetworkComponent()->GetHost()->IsLocal())
					{
						grcDebugDraw::AddDebugOutput("%s (Size: %u. Host: us)", name, size);
					}
					else 
					{
						grcDebugDraw::AddDebugOutput("%s (Size: %u. Host: %s)", name, size, handler->GetNetworkComponent()->GetHost()->GetLogName());
					}
				}
			}
		}

		grcDebugDraw::AddDebugOutput("");
	}
}

void CGameScriptHandlerMgr::DisplayObjectAndResourceInfo() const
{
	scriptHandlerMap::ConstIterator entry = m_scriptHandlers.CreateIterator();

	for (entry.Start(); !entry.AtEnd(); entry.Next())
	{
		CGameScriptHandler* handler = static_cast<CGameScriptHandler*>(entry.GetData()); 

		if (handler)
		{
			handler->DisplayObjectAndResourceInfo();
		}
	}
}

void CGameScriptHandlerMgr::DisplayReservationInfo() const
{
	u32 reservedPeds = 0;
	u32 reservedVehs = 0;
	u32 reservedObjs = 0;

	grcDebugDraw::AddDebugOutput("-- LOCAL SCRIPTS --");

	scriptHandlerMap::ConstIterator entry = m_scriptHandlers.CreateIterator();

	for (entry.Start(); !entry.AtEnd(); entry.Next())
	{
		CGameScriptHandler* handler = static_cast<CGameScriptHandler*>(entry.GetData()); 

		if (handler && handler->RequiresANetworkComponent() && !handler->IsTerminated())
		{
			const CGameScriptHandlerNetwork* netHandler = static_cast<const CGameScriptHandlerNetwork*>(handler);

			reservedPeds = netHandler->GetTotalNumReservedPeds();
			reservedVehs = netHandler->GetTotalNumReservedVehicles();
			reservedObjs = netHandler->GetTotalNumReservedObjects();

			if (reservedPeds > 0 || reservedVehs > 0 || reservedObjs > 0)
			{
				grcDebugDraw::AddDebugOutput("%s: %d/%d peds, %d/%d vehicles, %d/%d objects", netHandler->GetScriptName(), netHandler->GetNumCreatedPeds(), reservedPeds, netHandler->GetNumCreatedVehicles(), reservedVehs, netHandler->GetNumCreatedObjects(), reservedObjs);
			}
		}
	}

	grcDebugDraw::AddDebugOutput("-- REMOTE SCRIPTS --");

	PlayerFlags playersNearby = 0;
	PlayerFlags playersNearbyInPedScope = 0;
	PlayerFlags playersNearbyInVehScope = 0;
	PlayerFlags playersNearbyInObjScope = 0;

	bool playersNearbyCalculated = false;

	for (u32 i=0; i<m_remoteActiveScripts.GetCount(); i++)
	{
		const CRemoteScriptInfo* pInfo = &m_remoteActiveScripts[i];

		if (!pInfo->GetRunningLocally())
		{
			reservedPeds = 0;
			reservedVehs = 0;
			reservedObjs = 0;

			pInfo->GetReservations(reservedPeds, reservedVehs, reservedObjs);

			if (reservedPeds != 0 || reservedVehs != 0 || reservedObjs != 0)
			{
				if (!playersNearbyCalculated)
				{
					NetworkInterface::GetObjectManager().GetPlayersNearbyInScope(playersNearbyInPedScope, playersNearbyInVehScope, playersNearbyInObjScope);

					playersNearby = playersNearbyInPedScope | playersNearbyInVehScope | playersNearbyInObjScope;

					playersNearbyCalculated = true;
				}

				if (playersNearby != 0)
				{
					bool bParticipantNearbyInPedScope = false;
					bool bParticipantNearbyInVehScope = false;
					bool bParticipantNearbyInObjScope = false;

					for (u32 p=0; p<MAX_NUM_PHYSICAL_PLAYERS; p++)
					{
						u32 playerIndex = (1<<p);

						if ((playersNearby & playerIndex) && (pInfo->GetActiveParticipants() & playerIndex))
						{
							Assert(playersNearbyCalculated);

							if (playersNearbyInPedScope & playerIndex)
							{
								bParticipantNearbyInPedScope = true;
							}

							if (playersNearbyInVehScope & playerIndex)
							{
								bParticipantNearbyInVehScope = true;
							}

							if (playersNearbyInObjScope & playerIndex)
							{
								bParticipantNearbyInObjScope = true;
							}
						}
					}

					if (!bParticipantNearbyInPedScope)
					{
						reservedPeds = 0;
					}

					if (!bParticipantNearbyInVehScope)
					{
						reservedVehs = 0;
					}

					if (!bParticipantNearbyInObjScope)
					{
						reservedObjs = 0;
					}
				}
			}
	
			if (reservedPeds>0 || reservedVehs>0 || reservedObjs>0)
			{
				grcDebugDraw::AddDebugOutput("%s: %d/%d peds, %d/%d vehicles, %d/%d objects", pInfo->GetScriptId().GetScriptName(), pInfo->GetCreatedPeds(), reservedPeds, pInfo->GetCreatedVehicles(), reservedVehs, pInfo->GetCreatedObjects(), reservedObjs);
			}
		}
	}
}

#endif	//	__BANK


#if __SCRIPT_MEM_CALC
void CGameScriptHandlerMgr::CalculateMemoryUsage()
{
	const strIndex* ignoreList = NULL;
	s32 numIgnores = 0;

	GetResourceIgnoreList(&ignoreList, numIgnores);

	m_TotalScriptVirtualMemory = 0;
	m_TotalScriptPhysicalMemory = 0;

	u32 TotalVirtualForResourcesWithoutStreamingIndex = 0;
	u32 TotalPhysicalForResourcesWithoutStreamingIndex = 0;

	const int maxLoadedInt = strStreamingEngine::GetInfo().GetLoadedInfoMap()->GetNumSlots() + strStreamingEngine::GetInfo().GetLoadedPersistentInfoMap()->GetNumSlots()
			+ strStreamingEngine::GetInfo().GetRequestInfoMap()->GetNumSlots() + strStreamingEngine::GetInfo().GetLoadingInfoMap().GetNumSlots();

	// Fixes B* 2175673
	const int maxLoadedCount = CStreamVolumeMgr::GetStreamingIndicesCount();
	const u16 maxLoaded = (u16) Min(maxLoadedInt + maxLoadedCount, 0xffff);

	strIndex *streamingIndicesForAllScriptsBackingStore = Alloca(strIndex, maxLoaded);
	atUserArray<strIndex> streamingIndicesForAllScripts(streamingIndicesForAllScriptsBackingStore, maxLoaded);

	strIndex *streamingIndicesForThisScriptBackingStore = Alloca(strIndex, maxLoaded);
	atUserArray<strIndex> streamingIndicesForThisScript(streamingIndicesForThisScriptBackingStore, maxLoaded);

	u32 ScriptThreadId = 0;


	bool bDisplayScriptDetails = true;
	bool bFilterScriptObjectDisplay = false;
	bool bExcludeScripts = false;
	
#if __SCRIPT_MEM_DISPLAY
	bDisplayScriptDetails = CScriptDebug::GetDisplayDetailedScriptMemoryUsage();
	bFilterScriptObjectDisplay = CScriptDebug::GetDisplayDetailedScriptMemoryUsageFiltered();
	bExcludeScripts = CScriptDebug::GetExcludeScriptFromScriptMemoryUsage();
#endif	//	__SCRIPT_MEM_DISPLAY

	scriptHandlerMap::Iterator entry = m_scriptHandlers.CreateIterator();

	for (entry.Start(); !entry.AtEnd(); entry.Next())
	{
		CGameScriptHandler* handler = static_cast<CGameScriptHandler*>(entry.GetData()); 

		if (!handler) continue;

		streamingIndicesForThisScript.Resize(0);

		u32 VirtualForResourcesWithoutStreamingIndexInThisScript = 0;
		u32 PhysicalForResourcesWithoutStreamingIndexInThisScript = 0;

		ClearMemoryDebugInfoForMovieMeshes(true);

		handler->CalculateMemoryUsage(streamingIndicesForThisScript, ignoreList, numIgnores, 
			VirtualForResourcesWithoutStreamingIndexInThisScript, PhysicalForResourcesWithoutStreamingIndexInThisScript,
			bDisplayScriptDetails, bFilterScriptObjectDisplay, 0);	//STRFLAG_DONTDELETE);

		CalculateMemoryUsageForMovieMeshes(handler, VirtualForResourcesWithoutStreamingIndexInThisScript, PhysicalForResourcesWithoutStreamingIndexInThisScript, bDisplayScriptDetails);

		ScriptThreadId = 0;
		if (handler->GetThread())
		{
			ScriptThreadId = (u32) handler->GetThread()->GetThreadId();
		}

		if (ScriptThreadId != 0)
		{
			if (CStreamVolumeMgr::IsThereAVolumeOwnedBy((scrThreadId) ScriptThreadId))
			{
				CStreamVolumeMgr::GetStreamingIndices(streamingIndicesForThisScript);

#if __SCRIPT_MEM_DISPLAY
				if (bDisplayScriptDetails)
				{
					atArray<strIndex> streamingIndicesForStreamingVolume;
					CStreamVolumeMgr::GetStreamingIndices(streamingIndicesForStreamingVolume);

					u32 VirtualMemForVolume = 0;
					u32 PhysicalMemForVolume = 0;
					strStreamingEngine::GetInfo().GetObjectAndDependenciesSizes(streamingIndicesForStreamingVolume, VirtualMemForVolume, PhysicalMemForVolume, NULL, 0, true);
					grcDebugDraw::AddDebugOutput("%s(%u) Stream Volume %dK %dK", handler->GetScriptName(), ScriptThreadId, VirtualMemForVolume>>10, PhysicalMemForVolume>>10);
				}
#endif	//	__SCRIPT_MEM_DISPLAY
			}
		}

		//	Display the memory used by render targets for normal Bink movies (not mesh sets)
		CalculateMemoryUsageForNamedRenderTargets(handler, 
			VirtualForResourcesWithoutStreamingIndexInThisScript, PhysicalForResourcesWithoutStreamingIndexInThisScript,
			bDisplayScriptDetails);

#if __SCRIPT_MEM_DISPLAY
		if (CScriptDebug::GetDisplayScriptMemoryUsage())
		{
			u32 VirtualMemForThisScript = 0;
			u32 PhysicalMemForThisScript = 0;
			strStreamingEngine::GetInfo().GetObjectAndDependenciesSizes(streamingIndicesForThisScript, 
				VirtualMemForThisScript, PhysicalMemForThisScript, ignoreList, numIgnores, true);

			VirtualMemForThisScript += VirtualForResourcesWithoutStreamingIndexInThisScript;
			PhysicalMemForThisScript += PhysicalForResourcesWithoutStreamingIndexInThisScript;

			if (VirtualMemForThisScript > 0 || PhysicalMemForThisScript > 0)
			{
				grcDebugDraw::AddDebugOutput("%s(%u) Virtual %dK Physical %dK (for all resources, including deps)", 
					handler->GetScriptName(), ScriptThreadId, 
					VirtualMemForThisScript>>10, PhysicalMemForThisScript>>10);
			}
		}
#endif	//	__SCRIPT_MEM_DISPLAY

		// Add all entries from streamingIndicesForThisScript to streamingIndicesForAllScripts
		//	Should I remove duplicates at this stage?
		s32 indexCount = streamingIndicesForThisScript.GetCount();
		for (s32 i=0; i<indexCount; i++)
		{
			streamingIndicesForAllScripts.Append() = streamingIndicesForThisScript[i];
		}

		TotalVirtualForResourcesWithoutStreamingIndex += VirtualForResourcesWithoutStreamingIndexInThisScript;
		TotalPhysicalForResourcesWithoutStreamingIndex += PhysicalForResourcesWithoutStreamingIndexInThisScript;
	}

	strStreamingEngine::GetInfo().GetObjectAndDependenciesSizes(streamingIndicesForAllScripts, 
		m_TotalScriptVirtualMemory, m_TotalScriptPhysicalMemory, ignoreList, numIgnores, true);

	m_TotalScriptVirtualMemory += TotalVirtualForResourcesWithoutStreamingIndex;
	m_TotalScriptPhysicalMemory += TotalPhysicalForResourcesWithoutStreamingIndex;

	//looks for all the path nodes loaded for scripts
	u32 nTotalPathNodeVirtualMemory = 0;
	u32 nTotalPathNodePhysicalMemory = 0;
	ThePaths.CountScriptMemoryUsage(nTotalPathNodeVirtualMemory, nTotalPathNodePhysicalMemory);
 	m_TotalScriptVirtualMemory += nTotalPathNodeVirtualMemory;
 	m_TotalScriptPhysicalMemory += nTotalPathNodePhysicalMemory;

	//looks for all the navmeshes loaded for scripts
	u32 nTotalNavmeshVirtualMemory = 0;
	u32 nTotalNavmeshPhysicalMemory = 0;
	CPathServer::CountScriptMemoryUsage(nTotalNavmeshVirtualMemory, nTotalNavmeshPhysicalMemory);
	m_TotalScriptVirtualMemory += nTotalNavmeshVirtualMemory;
	m_TotalScriptPhysicalMemory += nTotalNavmeshPhysicalMemory;

	//looks for all the loaded scripts
	if(!bExcludeScripts)
	{
		u32 nTotalScriptVirtualMemory = 0;
		u32 nTotalScriptPhysicalMemory = 0;
		g_StreamedScripts.CalculateMemoryUsage(nTotalScriptVirtualMemory, nTotalScriptPhysicalMemory, bDisplayScriptDetails);
		m_TotalScriptVirtualMemory += nTotalScriptVirtualMemory;
		m_TotalScriptPhysicalMemory += nTotalScriptPhysicalMemory;
	}

	if (m_TotalScriptVirtualMemory > m_TotalScriptVirtualMemoryPeak)
	{
		m_TotalScriptVirtualMemoryPeak = m_TotalScriptVirtualMemory;
		if (PARAM_scriptLogMemoryPeaks.Get())
			Displayf("CGameScriptHandlerMgr : [SCRIPT][MEM_PEAK] : Virtual  : %s : %10dK", NetworkInterface::IsInFreeMode() ? "MP" : "SP", m_TotalScriptVirtualMemoryPeak>>10);
	}
	if (m_TotalScriptPhysicalMemory > m_TotalScriptPhysicalMemoryPeak)
	{
		m_TotalScriptPhysicalMemoryPeak = m_TotalScriptPhysicalMemory;
		if (PARAM_scriptLogMemoryPeaks.Get())
			Displayf("CGameScriptHandlerMgr : [SCRIPT][MEM_PEAK] : Physical : %s : %10dK", NetworkInterface::IsInFreeMode() ? "MP" : "SP", m_TotalScriptPhysicalMemoryPeak>>10);
	}

#if __SCRIPT_MEM_DISPLAY
	if (CScriptDebug::GetDisplayScriptMemoryUsage())
	{
		if (nTotalPathNodeVirtualMemory > 0 || nTotalPathNodePhysicalMemory > 0)
		{
			grcDebugDraw::AddDebugOutput("%s(%d) Virtual %dK Physical %dK", 
				"Script Path Nodes", -1, 
				nTotalPathNodeVirtualMemory>>10, nTotalPathNodePhysicalMemory>>10);
		}
	}
#endif	//	__SCRIPT_MEM_DISPLAY

	// periodically do a test to see if the script streaming memory has exceeded its limit
	if (m_streamingMemTestTimer.GetMsTime() > static_cast<float>(STREAMING_MEM_CHECK_TIME))
	{
#if __FINAL
		size_t virtualLimit = VIRTUAL_STREAMING_MEMORY_LIMIT;
		size_t physicalLimit = PHYSICAL_STREAMING_MEMORY_LIMIT;
#else
		size_t virtualLimit = VIRTUAL_STREAMING_MEMORY_LIMIT_DBG;
		size_t physicalLimit = PHYSICAL_STREAMING_MEMORY_LIMIT_DBG;
		const char *overrideScriptRpfName = NULL;
		PARAM_override_script.Get(overrideScriptRpfName);
		if(overrideScriptRpfName && !stricmp(overrideScriptRpfName, "script_rel"))
		{
			virtualLimit = VIRTUAL_STREAMING_MEMORY_LIMIT;
			physicalLimit = PHYSICAL_STREAMING_MEMORY_LIMIT;
		}
#endif

		if (NetworkInterface::IsInFreeMode() && !CPauseMenu::GetCurrenMissionActive() && !NetworkInterface::IsInSpectatorMode())
		{
			virtualLimit = VIRTUAL_STREAMING_FREEMODE_MEMORY_LIMIT;
			physicalLimit = PHYSICAL_STREAMING_FREEMODE_MEMORY_LIMIT;
		}

#if !ONE_STREAMING_HEAP
		if (((m_TotalScriptVirtualMemory>>10) > virtualLimit) || ((m_TotalScriptPhysicalMemory>>10) > physicalLimit))
#else
		if (((m_TotalScriptVirtualMemory>>10) + (m_TotalScriptPhysicalMemory>>10)) > virtualLimit +physicalLimit)
#endif 
		{
			m_lastStreamingEmergencyTimer.Reset();
		}

		// check if streaming is trying to request too much data at once
		CPlayerInfo *pPlayerInfo = CGameWorld::GetMainPlayerInfo();
		if (pPlayerInfo && !pPlayerInfo->AreControlsDisabled())
		{
			bool emergencyHit = false;
			s32 indexCount = streamingIndicesForAllScripts.GetCount();
			for (s32 i = 0; i < indexCount; ++i)
			{
				const strStreamingInfo& info = strStreamingEngine::GetInfo().GetStreamingInfoRef(streamingIndicesForAllScripts[i]);
				if (info.GetStatus() != STRINFO_LOADING && info.GetStatus() != STRINFO_LOADREQUESTED)
				{
					streamingIndicesForAllScripts[i] = strIndex(strIndex::INVALID_INDEX);
				}
			}
			u32 virt = 0;
			u32 phys = 0;
			strStreamingEngine::GetInfo().GetObjectAndDependenciesSizes(streamingIndicesForAllScripts, virt, phys, ignoreList, numIgnores, true);
			if ((virt + phys) >> 10 > TOTAL_STREAMING_REQUEST_TOTAL_LIMIT)
			{
				m_lastStreamingReqEmergencyTimer.Reset();
				emergencyHit = true;
			}

			// check priority requests only
			if (!emergencyHit)
			{
				for (s32 i = 0; i < indexCount; ++i)
				{
					if (streamingIndicesForAllScripts[i] == strIndex(strIndex::INVALID_INDEX))
						continue;

					const strStreamingInfo& info = strStreamingEngine::GetInfo().GetStreamingInfoRef(streamingIndicesForAllScripts[i]);
					if ((info.GetFlags() & STRFLAG_PRIORITY_LOAD) == 0)
					{
						streamingIndicesForAllScripts[i] = strIndex(strIndex::INVALID_INDEX);
					}
				}
				u32 virt = 0;
				u32 phys = 0;
				strStreamingEngine::GetInfo().GetObjectAndDependenciesSizes(streamingIndicesForAllScripts, virt, phys, ignoreList, numIgnores, true);
				if ((virt + phys) >> 10 > TOTAL_STREAMING_REQUEST_PRIO_LIMIT)
				{
					m_lastStreamingReqEmergencyTimer.Reset();
				}
			}

			m_streamingMemTestTimer.Reset();
		}
	}
}

void CGameScriptHandlerMgr::ClearMemoryDebugInfoForMovieMeshes(bool bTrackMemoryForMovieMeshes) const
{
	m_bTrackMemoryForMovieMeshes = bTrackMemoryForMovieMeshes;
	m_ArrayOfModelsUsedByMovieMeshes.Reset();
	m_MapOfMoviesUsedByMovieMeshes.Reset();
	m_MapOfRenderTargetsUsedByMovieMeshes.Reset();
}

void CGameScriptHandlerMgr::CalculateMemoryUsageForMovieMeshes(const CGameScriptHandler* SCRIPT_MEM_DISPLAY_ONLY(pScriptHandler), u32 &VirtualTotal, u32 &PhysicalTotal, bool SCRIPT_MEM_DISPLAY_ONLY(bDisplayScriptDetails)) const
{
//	Models
	u32 NumberOfModels = m_ArrayOfModelsUsedByMovieMeshes.GetCount();
	if (NumberOfModels > 0)
	{
		s32 totalPhysicalMemory = 0;
		s32 totalVirtualMemory = 0;
		s32 drawablePhysicalMemory = 0;
		s32 drawableVirtualMemory = 0;
		s32 texturePhysicalMemory = 0;
		s32 textureVirtualMemory = 0;
		s32 boundsPhysicalMemory = 0;
		s32 boundsVirtualMemory = 0;

		CStreamingDebug::GetMemoryStatsByAllocation(m_ArrayOfModelsUsedByMovieMeshes, totalPhysicalMemory, totalVirtualMemory, drawablePhysicalMemory, drawableVirtualMemory, 
			texturePhysicalMemory, textureVirtualMemory, boundsPhysicalMemory, boundsVirtualMemory);

		VirtualTotal += totalVirtualMemory;
		PhysicalTotal += totalPhysicalMemory;

#if __SCRIPT_MEM_DISPLAY
		if (bDisplayScriptDetails)
		{
			grcDebugDraw::AddDebugOutput("%s(%d) Total For %u Models in Mesh Sets : Virtual %dK Physical %dK", 
				pScriptHandler->GetScriptName(), pScriptHandler->GetThread()->GetThreadId(), 
				NumberOfModels, 
				totalVirtualMemory>>10, totalPhysicalMemory>>10);
		}
#endif	//	__SCRIPT_MEM_DISPLAY
	}

//	Movies
	u32 virtualSize = 0;
	u32 physicalSize = 0;

	u32 NumberOfMovies = m_MapOfMoviesUsedByMovieMeshes.GetNumUsed();
	if (NumberOfMovies > 0)
	{
#if __SCRIPT_MEM_DISPLAY
		if (bDisplayScriptDetails)
		{
			grcDebugDraw::AddDebugOutput("%s(%d) Movies used in Mesh Sets", 
				pScriptHandler->GetScriptName(), pScriptHandler->GetThread()->GetThreadId());
		}
#endif	//	__SCRIPT_MEM_DISPLAY

		atMap<u32,u32>::Iterator MovieMapIterator = m_MapOfMoviesUsedByMovieMeshes.CreateIterator();
		while (!MovieMapIterator.AtEnd())
		{
			u32 movieHandle = MovieMapIterator.GetKey();

			//	Is there a way to get the name of the movie from its handle?

			u32 virtSizeForThisMovie = g_movieMgr.GetMemoryUsage(movieHandle);
			virtualSize += virtSizeForThisMovie;

#if __SCRIPT_MEM_DISPLAY
			if (bDisplayScriptDetails)
			{
				grcDebugDraw::AddDebugOutput("Movie Handle=%u: used %d times : Virtual %dK", 
					movieHandle, MovieMapIterator.GetData(),
					virtSizeForThisMovie>>10);
			}
#endif	//	__SCRIPT_MEM_DISPLAY

			MovieMapIterator.Next();
		}

		VirtualTotal += virtualSize;
		PhysicalTotal += physicalSize;

#if __SCRIPT_MEM_DISPLAY
		if (bDisplayScriptDetails)
		{
			grcDebugDraw::AddDebugOutput("%s(%d) Total For Movies in Mesh Sets : Virtual %dK", 
				pScriptHandler->GetScriptName(), pScriptHandler->GetThread()->GetThreadId(), 
				virtualSize>>10);
		}
#endif	//	__SCRIPT_MEM_DISPLAY
	}

//	Render Targets
	virtualSize = 0;
	physicalSize = 0;

	u32 NumberOfRenderTargets = m_MapOfRenderTargetsUsedByMovieMeshes.GetNumUsed();
	if (NumberOfRenderTargets > 0)
	{
#if __SCRIPT_MEM_DISPLAY
		if (bDisplayScriptDetails)
		{
			grcDebugDraw::AddDebugOutput("%s(%d) Render Targets used in Mesh Sets", 
				pScriptHandler->GetScriptName(), pScriptHandler->GetThread()->GetThreadId());
		}
#endif	//	__SCRIPT_MEM_DISPLAY

		atMap<CRenderTargetMgr::namedRendertarget*, u32>::Iterator RenderTargetIterator = m_MapOfRenderTargetsUsedByMovieMeshes.CreateIterator();
		while (!RenderTargetIterator.AtEnd())
		{
			CRenderTargetMgr::namedRendertarget *pRenderTarget = RenderTargetIterator.GetKey();

			if (pRenderTarget)
			{
				u32 SizeForThisMovie = pRenderTarget->GetMemoryUsage();
				physicalSize += SizeForThisMovie;

#if __SCRIPT_MEM_DISPLAY
				if (bDisplayScriptDetails)
				{
					grcDebugDraw::AddDebugOutput("Render Target=%x: used %d times : Physical %dK", 
						pRenderTarget, RenderTargetIterator.GetData(),
						SizeForThisMovie>>10);
				}
#endif	//	__SCRIPT_MEM_DISPLAY
			}

			RenderTargetIterator.Next();
		}

		VirtualTotal += virtualSize;
		PhysicalTotal += physicalSize;

#if __SCRIPT_MEM_DISPLAY
		if (bDisplayScriptDetails)
		{
			grcDebugDraw::AddDebugOutput("%s(%d) Total For RenderTargets in Mesh Sets : Physical %dK", 
				pScriptHandler->GetScriptName(), pScriptHandler->GetThread()->GetThreadId(), 
				physicalSize>>10);
		}
#endif	//	__SCRIPT_MEM_DISPLAY
	}

	ClearMemoryDebugInfoForMovieMeshes(false);
}

//	Display the memory used by render targets for normal Bink movies (not mesh sets)
void CGameScriptHandlerMgr::CalculateMemoryUsageForNamedRenderTargets(const CGameScriptHandler* pScriptHandler, u32& UNUSED_PARAM(virtualMemory), u32 &physicalMemory, bool SCRIPT_MEM_DISPLAY_ONLY(bDisplayScriptDetails)) const
{
	if (pScriptHandler->GetThread())
	{
		u32 physicalMemoryForThisThread = gRenderTargetMgr.GetMemoryUsageForNamedRenderTargets(pScriptHandler->GetThread()->GetThreadId());
		physicalMemory += physicalMemoryForThisThread;

#if __SCRIPT_MEM_DISPLAY
		if (bDisplayScriptDetails && (physicalMemoryForThisThread > 0))
		{
			grcDebugDraw::AddDebugOutput("%s(%d) Total For RenderTargets in Movies (excluding movies used by mesh sets) : Physical %dK", 
				pScriptHandler->GetScriptName(), pScriptHandler->GetThread()->GetThreadId(), 
				physicalMemoryForThisThread>>10);
		}
#endif	//	__SCRIPT_MEM_DISPLAY
	}
}

void CGameScriptHandlerMgr::AddToModelsUsedByMovieMeshes(u32 ModelIndex)
{
	if (m_bTrackMemoryForMovieMeshes)
	{
		m_ArrayOfModelsUsedByMovieMeshes.PushAndGrow(ModelIndex);
	}
}

void CGameScriptHandlerMgr::AddToMoviesUsedByMovieMeshes(u32 MovieHandle)
{
	if (m_bTrackMemoryForMovieMeshes)
	{
		u32 *pNumberOfTimesUsed = m_MapOfMoviesUsedByMovieMeshes.Access(MovieHandle);
		if (pNumberOfTimesUsed)
		{
			m_MapOfMoviesUsedByMovieMeshes[MovieHandle] = 1 + *pNumberOfTimesUsed;
		}
		else
		{
			m_MapOfMoviesUsedByMovieMeshes.Insert(MovieHandle, 1);
		}
	}
}

void CGameScriptHandlerMgr::AddToRenderTargetsUsedByMovieMeshes(CRenderTargetMgr::namedRendertarget *pNamedRenderTarget)
{
	if (m_bTrackMemoryForMovieMeshes)
	{
		u32 *pNumberOfTimesUsed = m_MapOfRenderTargetsUsedByMovieMeshes.Access(pNamedRenderTarget);
		if (pNumberOfTimesUsed)
		{
			m_MapOfRenderTargetsUsedByMovieMeshes[pNamedRenderTarget] = 1 + *pNumberOfTimesUsed;
		}
		else
		{
			m_MapOfRenderTargetsUsedByMovieMeshes.Insert(pNamedRenderTarget, 1);
		}
	}
}
#endif // __SCRIPT_MEM_CALC

#if __BANK

void CGameScriptHandlerMgr::GetScriptDependencyList(atArray<strIndex> &theArray)
{
	atArray<strIndex> localIndices;

	const strIndex* ignoreList = NULL;
	s32 numIgnores = 0;

	GetResourceIgnoreList(&ignoreList, numIgnores);

	// ignore vehicle resident texture dictionaries. Not sure why (copied from old mission cleanup code)
	scriptHandlerMap::Iterator entry = m_scriptHandlers.CreateIterator();

	for (entry.Start(); !entry.AtEnd(); entry.Next())
	{
		CGameScriptHandler* handler = static_cast<CGameScriptHandler*>(entry.GetData()); 

		if (handler)
		{
			handler->GetStreamingIndices(localIndices, STRFLAG_DONTDELETE);
		}
	}

	for(int i=0;i<localIndices.size();i++)
	{
		strStreamingEngine::GetInfo().GetObjectAndDependencies(localIndices[i], theArray, ignoreList, numIgnores);
	}
}

void CGameScriptHandlerMgr::UpdateScriptHandlerCombo()
{
#if __DEV
	if (ms_pHandlerCombo)
	{
		ms_NumHandlers = 0;

		scriptHandlerMap::Iterator entry = m_scriptHandlers.CreateIterator();

		for (entry.Start(); !entry.AtEnd(); entry.Next())
		{
			CGameScriptHandler* handler = static_cast<CGameScriptHandler*>(entry.GetData()); 

			if (handler && handler->GetScriptName() && AssertVerify(ms_NumHandlers<CGameScriptHandler::MAX_NUM_SCRIPT_HANDLERS))
			{
				ms_Handlers[ms_NumHandlers++] = handler->GetScriptName();
			}
		}

		if (ms_NumHandlers == 0)
		{
			ms_Handlers[0] = "-- none --";
			ms_NumHandlers = 1;
		}

		ms_pHandlerCombo->UpdateCombo("Script Handlers", &ms_HandlerComboIndex, ms_NumHandlers, (const char **)ms_Handlers);
	}
#endif // __DEV
}

const CGameScriptHandler* CGameScriptHandlerMgr::GetScriptHandlerFromComboName(const char* scriptName) const
{
	scriptHandlerMap::ConstIterator entry = m_scriptHandlers.CreateIterator();

	for (entry.Start(); !entry.AtEnd(); entry.Next())
	{
		CGameScriptHandler* handler = static_cast<CGameScriptHandler*>(entry.GetData()); 

		if (handler && handler->GetScriptName() && !strcmp(handler->GetScriptName(), scriptName))
		{
			return handler;
		}
	}

	return NULL;
}

CGameScriptHandler* CGameScriptHandlerMgr::GetScriptHandlerFromComboName(const char* scriptName)
{
	scriptHandlerMap::Iterator entry = m_scriptHandlers.CreateIterator();

	for (entry.Start(); !entry.AtEnd(); entry.Next())
	{
		CGameScriptHandler* handler = static_cast<CGameScriptHandler*>(entry.GetData()); 

        if (handler && handler->GetScriptName() && !strcmp(handler->GetScriptName(), scriptName))
        {
            return handler;
        }
    }

    return NULL;
}

#endif // __BANK

// gets the streaming objects to ignore when calculating or displaying resource memory usage
void CGameScriptHandlerMgr::GetResourceIgnoreList(const strIndex** ignoreList, s32& numIgnores) const
{
	const atArray<strIndex>& residentIgnoreList = gPopStreaming.GetResidentObjectList();
	*ignoreList = residentIgnoreList.GetElements();
	numIgnores = residentIgnoreList.GetCount();
}

void CGameScriptHandlerMgr::WriteRemoteScriptHeader(const CGameScriptId& LOGGING_ONLY(scriptId), const char *LOGGING_ONLY(headerText), ...)
{
#if ENABLE_NETWORK_LOGGING
	char buffer[netLog::MAX_LOG_STRING];
	va_list args;
	va_start(args, headerText);
	vsprintf(buffer, headerText, args);
	va_end(args);

    if(GetLog())
    {
	    GetLog()->Log("\t<remote script update> %s: %s\r\n", scriptId.GetLogName(), buffer);
    }
#endif // #if ENABLE_NETWORK_LOGGING
}

bool CGameScriptHandlerMgr::HandleTimestampConflict(const CRemoteScriptInfo& scriptInfo)
{
	bool bAddInfo = true;
	bool bDeleteExistingInfo = false;

	// watches out for 2 identical scripts running at once with the same timestamp.

	// this can happen for 2 reasons:
	// 1) a joining machine starts up and hosts a script immediately before being aware of other machines in the session (and the real host). 
	// This really shouldn't happen: the joining machine should wait for the timeout time before starting up any scripts but we still need to handle 
	// this problem in case it does arise.
	// 2) a new player is accepted by a host running the script on his own, who drops out immediately. The new player loses the host during the joining 
	// process and starts a new session. The host has not broadcast a remote script info with 0 participants as he thinks there is a remaining 
	// participant (the new player).

	CGameScriptId remoteScriptId = scriptInfo.GetScriptId();
	unsigned remoteTimeStamp = remoteScriptId.GetTimeStamp();

	const CGameScriptId* pExistingScriptId = NULL;
	u32 existingInfoSlot = 0;

	// set timestamp to 0 so that it matches ids of existing script handlers with differing timestamps 
	remoteScriptId.ResetTimestamp();

	// find an existing remote script info for this script
	for (u32 i=0; i<m_remoteActiveScripts.GetCount(); i++)
	{
		if (m_remoteActiveScripts[i].m_ScriptId == remoteScriptId && m_remoteActiveScripts[i].GetActiveParticipants() != 0)
		{
			pExistingScriptId = &m_remoteActiveScripts[i].GetScriptId();

			unsigned existingTimestamp = pExistingScriptId->GetTimeStamp();

			if (gnetVerify(existingTimestamp != remoteTimeStamp))
			{
				WriteRemoteScriptHeader(*pExistingScriptId, "** TIMESTAMP CONFLICT (Remote timestamp: %d) **", remoteTimeStamp);

				if (remoteTimeStamp > existingTimestamp)
				{
#if __ASSERT
					// we should not have an active handler when we get a timestamp conflict
					scriptHandler* pHandler = GetScriptHandler(*pExistingScriptId);

					if (pHandler && 
						pHandler->GetNetworkComponent() && 
						pHandler->GetNetworkComponent()->GetState() == scriptHandlerNetComponent::NETSCRIPT_PLAYING)
					{
						gnetAssertf(0, "Two versions of script %s active with different timestamps! (Remote timestamp: %d)", pExistingScriptId->GetLogName(), remoteTimeStamp);
						return true;
					}
#endif
					WriteRemoteScriptHeader(*pExistingScriptId, "Info removed");

					bDeleteExistingInfo = true;
					existingInfoSlot = i;
				}
				else
				{
					WriteRemoteScriptHeader(scriptInfo.GetScriptId(), "Info not added");

					// don't add this script info
					bAddInfo = false;
				}
			}

			break;
		}
	}

	if (bDeleteExistingInfo)
	{
		MoveActiveScriptToTerminatedQueue(existingInfoSlot);
	}

	return bAddInfo;
}

void CGameScriptHandlerMgr::MoveActiveScriptToTerminatedQueue(u32 activeScriptSlot)
{
	CRemoteScriptInfo scriptInfo = m_remoteActiveScripts[activeScriptSlot];

	scriptInfo.SetParticipants(0);
	scriptInfo.SetHost(NULL);

	WriteRemoteScriptHeader(scriptInfo.GetScriptId(), "Remote script has terminated, move to terminated queue");	

	m_remoteActiveScripts.Delete(activeScriptSlot);

	scriptHandler* pScriptHandler = GetScriptHandler(scriptInfo.GetScriptId());

	// ignore the handler if it is running with a 0 timestamp. We only care about the previous instance of the script.
	bool bIgnoreZeroTimestamp = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("CLEANUP_SCRIPT_OBJECTS_FIX", 0x03bfbf68), true);

	if (!pScriptHandler || (bIgnoreZeroTimestamp && static_cast<const CGameScriptId&>(pScriptHandler->GetScriptId()).GetTimeStamp() == 0))
	{
		WriteRemoteScriptHeader(scriptInfo.GetScriptId(), "Cleanup objects, script not running locally");	

		// inform the object mgr that this script has terminated, so it can clean up any script objects that belonged to the script
		NetworkInterface::GetObjectManager().CleanupScriptObjects(scriptInfo.GetScriptId());
	}

#if __ASSERT
	for (u32 i=0; i<m_remoteTerminatedScripts.GetCount(); i++)
	{
		if (m_remoteTerminatedScripts[i].m_ScriptId == scriptInfo.GetScriptId())
		{
			Assertf(0, "Remote script info %s already on terminated queue", scriptInfo.GetScriptId().GetLogName());
			return;
		}
	}
#endif // __ASSERT

	if (m_remoteTerminatedScripts.IsFull())
		m_remoteTerminatedScripts.Pop();

	if (!m_remoteTerminatedScripts.Push(scriptInfo))
	{
		Assertf(0, "Failed to add a terminated script to the remote terminated script queue");
	}
}
