

/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    ModelIndices.h
// PURPOSE : This file is a bit of a hack really. It contains #defines for
//			 the modelindices of some of the objects as they come out of max.
//			 These numbers change occasionally and rather than having to
//			 go through the code it is easier to change them all in this
//			 one file.
// AUTHOR :  Obbe.
// CREATED : 3/4/00
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef _MODELINDICES_H_
#define _MODELINDICES_H_

// Rage headers
#include "streaming/streamingmodule.h"

// Game headers
#include "debug/debug.h"
#include "entity/entity.h"

//
// name:		CModelIndex
// description:	Class for storing model index. It is initialized with a name, and then this is converted once all models are loaded
class CModelIndex
{
public:
	CModelIndex(const strStreamingObjectName& pName);
	

	static void MatchAllModelStrings();

	operator u32() const {return m_index;}
	operator fwModelId() const { return(fwModelId(strLocalIndex(m_index))); }

	void Set(u32 index, const strStreamingObjectName& name) { m_index = index; m_pName = name; }
	u32  GetModelIndex() const { Assert(m_index!= fwModelId::MI_INVALID); return(m_index); }

	bool IsValid()			{ return (m_index != fwModelId::MI_INVALID); }

    strStreamingObjectName GetName() const {return m_pName;}

	CModelIndex(const CModelIndex& ) {Assert(0);}
	CModelIndex& operator=(const CModelIndex& val) {Assert(0); return const_cast<CModelIndex&>(val);}

private:

	strStreamingObjectName m_pName;
	u32 m_index;
	//fwModelId m_modelId;
	CModelIndex* m_pNext;

	static CModelIndex* sm_pFirst;
	static bool sm_matchedStrings;
};

// Objects:
extern CModelIndex	MI_CELLPHONE;
extern CModelIndex	MI_WINDMILL;

extern CModelIndex MI_PHONEBOX_4;

extern CModelIndex MI_LRG_GATE_L;
extern CModelIndex MI_LRG_GATE_R;

extern CModelIndex MI_GROUND_LIGHT_RED;
extern CModelIndex MI_GROUND_LIGHT_GREEN;
extern CModelIndex MI_GROUND_LIGHT_YELLOW;

// Railway crossing barriers:
extern CModelIndex MI_RAILWAY_CROSSING_BARRIER_1;
extern CModelIndex MI_RAILWAY_BARRIER_01;

// Props with env cloth, requiring avoiding
extern CModelIndex MI_PROP_FNCLINK_04J;
extern CModelIndex MI_PROP_FNCLINK_04K;
extern CModelIndex MI_PROP_FNCLINK_04L;
extern CModelIndex MI_PROP_FNCLINK_04M;
extern CModelIndex MI_PROP_FNCLINK_02I;
extern CModelIndex MI_PROP_FNCLINK_02J;
extern CModelIndex MI_PROP_FNCLINK_02K;
extern CModelIndex MI_PROP_FNCLINK_02L;
extern CModelIndex MI_PROP_AIR_WINDSOCK;
extern CModelIndex MI_PROP_AIR_CARGO_01A;
extern CModelIndex MI_PROP_AIR_CARGO_01B;
extern CModelIndex MI_PROP_AIR_CARGO_01C;
extern CModelIndex MI_PROP_AIR_CARGO_02A;
extern CModelIndex MI_PROP_AIR_CARGO_02B;
extern CModelIndex MI_PROP_AIR_TRAILER_1A;
extern CModelIndex MI_PROP_AIR_TRAILER_1B;
extern CModelIndex MI_PROP_AIR_TRAILER_1C;
extern CModelIndex MI_PROP_SKID_TENT_CLOTH;
extern CModelIndex MI_PROP_SKID_TENT_01;
extern CModelIndex MI_PROP_SKID_TENT_01B;
extern CModelIndex MI_PROP_SKID_TENT_03;

extern CModelIndex MI_PROP_BEACH_PARASOL_01;
extern CModelIndex MI_PROP_BEACH_PARASOL_02;
extern CModelIndex MI_PROP_BEACH_PARASOL_03;
extern CModelIndex MI_PROP_BEACH_PARASOL_04;
extern CModelIndex MI_PROP_BEACH_PARASOL_05;
extern CModelIndex MI_PROP_BEACH_PARASOL_06;
extern CModelIndex MI_PROP_BEACH_PARASOL_07;
extern CModelIndex MI_PROP_BEACH_PARASOL_08;
extern CModelIndex MI_PROP_BEACH_PARASOL_09;
extern CModelIndex MI_PROP_BEACH_PARASOL_10;

// Money bag used in Capture jobs:
extern CModelIndex MI_MONEY_BAG;

// Cars:
extern CModelIndex	MI_CAR_VOLTIC;
extern CModelIndex	MI_CAR_VOLTIC2;
extern CModelIndex	MI_CAR_TORNADO;
extern CModelIndex  MI_CAR_TORNADO6;
// Law enforcement
extern CModelIndex	MI_CAR_CRUSADER;
extern CModelIndex	MI_CAR_POLICE;
extern CModelIndex	MI_CAR_POLICE_2;
extern CModelIndex	MI_CAR_POLICE_3;
extern CModelIndex	MI_CAR_SHERIFF;
extern CModelIndex	MI_CAR_POLICE_CURRENT;
extern CModelIndex	MI_CAR_NOOSE;
extern CModelIndex	MI_CAR_NOOSE_JEEP;
extern CModelIndex	MI_CAR_NOOSE_JEEP_2;
extern CModelIndex	MI_CAR_FBI;
extern CModelIndex	MI_CAR_CLASSIC_TAXI;
extern CModelIndex	MI_CAR_TAXI_CURRENT_1;
extern CModelIndex	MI_CAR_SECURICAR;
extern CModelIndex  MI_VAN_RIOT;
extern CModelIndex  MI_VAN_RIOT_2;

// Others
extern CModelIndex	MI_CAR_AMBULANCE;
extern CModelIndex	MI_CAR_FORKLIFT;
extern CModelIndex	MI_CAR_HANDLER;
extern CModelIndex	MI_CAR_FIRETRUCK;
extern CModelIndex	MI_CAR_GARBAGE_COLLECTION;
extern CModelIndex  MI_CAR_TAILGATER;
extern CModelIndex	MI_CAR_TOWTRUCK;
extern CModelIndex	MI_CAR_TOWTRUCK_2;
extern CModelIndex	MI_CAR_MONROE;
extern CModelIndex	MI_CAR_ZTYPE;
extern CModelIndex	MI_CAR_CADDY;
extern CModelIndex	MI_CAR_CADDY3;
extern CModelIndex	MI_CAR_HAULER;
extern CModelIndex	MI_CAR_MESA3;
extern CModelIndex	MI_CAR_SPEEDO;
extern CModelIndex	MI_CAR_SPEEDO2;
extern CModelIndex	MI_CAR_BTYPE;
extern CModelIndex	MI_CAR_BTYPE3;
extern CModelIndex	MI_CAR_ZENTORNO;
extern CModelIndex	MI_CAR_CYCLONE2;
extern CModelIndex  MI_CAR_CORSITA;
extern CModelIndex	MI_CAR_PANTO;
extern CModelIndex	MI_CAR_GLENDALE;
extern CModelIndex	MI_CAR_PIGALLE;
extern CModelIndex  MI_CAR_COQUETTE2;
extern CModelIndex  MI_CAR_RATLOADER;
extern CModelIndex  MI_CAR_BUCCANEER2;
extern CModelIndex  MI_CAR_KURUMA2;
extern CModelIndex	MI_CAR_BRAWLER;
extern CModelIndex  MI_CAR_INSURGENT;
extern CModelIndex  MI_CAR_LIMO2;
extern CModelIndex  MI_CAR_MAMBA;
extern CModelIndex	MI_TRAILER_TRAILER;
extern CModelIndex	MI_TRAILER_TR2;
extern CModelIndex  MI_TRAILER_TR4;
extern CModelIndex	MI_BIKE_SANCHEZ;
extern CModelIndex	MI_BIKE_SANCHEZ2;
extern CModelIndex	MI_BIKE_BMX;
extern CModelIndex	MI_BIKE_SCORCHER;
extern CModelIndex  MI_BIKE_THRUST;
extern CModelIndex  MI_BIKE_BF400;
extern CModelIndex  MI_BIKE_GARGOYLE;
extern CModelIndex	MI_BIKE_SHOTARO;
extern CModelIndex	MI_BIKE_SANCTUS;
extern CModelIndex	MI_BIKE_ESSKEY;
extern CModelIndex	MI_BIKE_OPPRESSOR;
extern CModelIndex	MI_HELI_CARGOBOB;
extern CModelIndex	MI_HELI_CARGOBOB2;
extern CModelIndex	MI_HELI_CARGOBOB3;
extern CModelIndex	MI_HELI_CARGOBOB4;
extern CModelIndex	MI_HELI_CARGOBOB5;
extern CModelIndex	MI_CAR_BENSON_TRUCK;
extern CModelIndex	MI_PROP_TRAILER;
extern CModelIndex	MI_PLANE_JET;
extern CModelIndex	MI_PLANE_TITAN;
extern CModelIndex	MI_PLANE_CARGOPLANE;
extern CModelIndex	MI_PLANE_MAMMATUS;
extern CModelIndex	MI_PLANE_DODO;
extern CModelIndex	MI_PLANE_VESTRA;
extern CModelIndex	MI_PLANE_STUNT;
extern CModelIndex	MI_PLANE_LUXURY_JET;
extern CModelIndex	MI_PLANE_LUXURY_JET2;
extern CModelIndex	MI_PLANE_LUXURY_JET3;
extern CModelIndex	MI_PLANE_DUSTER;
extern CModelIndex  MI_PLANE_VELUM;
extern CModelIndex  MI_PLANE_VELUM2;
extern CModelIndex  MI_PLANE_BESRA;
extern CModelIndex  MI_PLANE_MILJET;
extern CModelIndex  MI_PLANE_CUBAN;
extern CModelIndex  MI_PLANE_HYDRA;
extern CModelIndex  MI_PLANE_NIMBUS;
extern CModelIndex	MI_PLANE_TULA;
extern CModelIndex	MI_PLANE_MICROLIGHT;
extern CModelIndex  MI_PLANE_BOMBUSHKA;
extern CModelIndex	MI_PLANE_MOGUL;
extern CModelIndex	MI_PLANE_STARLING;
extern CModelIndex	MI_PLANE_PYRO;
extern CModelIndex	MI_PLANE_NOKOTA;
extern CModelIndex	MI_PLANE_HOWARD;
extern CModelIndex	MI_PLANE_ROGUE;
extern CModelIndex	MI_PLANE_MOLOTOK;
extern CModelIndex	MI_PLANE_SEABREEZE;
extern CModelIndex	MI_PLANE_AVENGER;

extern CModelIndex	MI_HELI_HAVOK;
extern CModelIndex  MI_HELI_HUNTER;

extern CModelIndex MI_HELI_BUZZARD;
extern CModelIndex MI_HELI_BUZZARD2;

extern CModelIndex	MI_TRAILER_TANKER;
extern CModelIndex	MI_TRAILER_TANKER2;

extern CModelIndex	MI_TRAILER_TRAILERLARGE;
extern CModelIndex	MI_TRAILER_TRAILERSMALL2;

extern CModelIndex  MI_TRAIN_METRO;
extern CModelIndex	MI_MONSTER_TRUCK;
extern CModelIndex	MI_HELI_DRONE;
extern CModelIndex	MI_HELI_DRONE_2;
extern CModelIndex	MI_HELI_VALKYRIE;
extern CModelIndex	MI_HELI_VALKYRIE2;
extern CModelIndex	MI_HELI_SWIFT;
extern CModelIndex	MI_HELI_SWIFT2;
extern CModelIndex	MI_HELI_SAVAGE;
extern CModelIndex	MI_HELI_SUPER_VOLITO;
extern CModelIndex	MI_HELI_SUPER_VOLITO2;
extern CModelIndex	MI_HELI_VOLATUS;

extern CModelIndex	MI_JETPACK_THRUSTER;

extern CModelIndex	MI_ROLLER_COASTER_CAR_1;
extern CModelIndex	MI_ROLLER_COASTER_CAR_2;

extern CModelIndex	MI_CAR_CARACARA;

extern CModelIndex	MI_CAR_SWINGER;
extern CModelIndex	MI_CAR_MULE4;
extern CModelIndex	MI_CAR_SPEEDO4;
extern CModelIndex	MI_CAR_POUNDER2;
extern CModelIndex	MI_CAR_PATRIOT2;
extern CModelIndex  MI_BATTLE_DRONE_QUAD;
extern CModelIndex	MI_BIKE_OPPRESSOR2;
extern CModelIndex	MI_CAR_PBUS2;
extern CModelIndex	MI_PLANE_STRIKEFORCE;
extern CModelIndex	MI_CAR_MENACER;
extern CModelIndex	MI_CAR_TERBYTE;
extern CModelIndex	MI_CAR_STAFFORD;
extern CModelIndex  MI_CAR_SCRAMJET;

extern CModelIndex	MI_CAR_SCARAB;
extern CModelIndex	MI_CAR_SCARAB2;
extern CModelIndex	MI_CAR_SCARAB3;
extern CModelIndex	MI_CAR_RCBANDITO;
extern CModelIndex	MI_CAR_BRUISER;
extern CModelIndex	MI_TRUCK_CERBERUS;
extern CModelIndex	MI_TRUCK_CERBERUS2;
extern CModelIndex	MI_TRUCK_CERBERUS3;
extern CModelIndex	MI_BIKE_DEATHBIKE;
extern CModelIndex  MI_BIKE_DEATHBIKE2;

extern CModelIndex MI_CAR_DYNASTY;
extern CModelIndex MI_CAR_EMERUS; 
extern CModelIndex MI_CAR_HELLION;
extern CModelIndex MI_CAR_LOCUST;
extern CModelIndex MI_CAR_NEO;
extern CModelIndex MI_CAR_NOVAK;
extern CModelIndex MI_CAR_PEYOTE2;
extern CModelIndex MI_CAR_S80;
extern CModelIndex MI_CAR_ZION3;
extern CModelIndex MI_CAR_ZORRUSSO;

extern CModelIndex  MI_CAR_PBUS;

extern CModelIndex	MI_TANK_RHINO;
extern CModelIndex	MI_TANK_KHANJALI;

extern CModelIndex	MI_CAR_TACO;
extern CModelIndex	MI_CAR_BULLDOZER;

extern CModelIndex  MI_CAR_BANSHEE2;
extern CModelIndex  MI_CAR_SLAMVAN3;

extern CModelIndex  MI_CAR_PACKER;
extern CModelIndex  MI_CAR_PHANTOM;
extern CModelIndex  MI_CAR_BRICKADE;

extern CModelIndex	MI_CAR_CHERNOBOG;

extern CModelIndex  MI_CAR_DUMP_TRUCK;
extern CModelIndex  MI_CAR_TROPHY_TRUCK;
extern CModelIndex  MI_CAR_TROPHY_TRUCK2;
extern CModelIndex  MI_CAR_TROPOS;
extern CModelIndex	MI_CAR_OMNIS;

extern CModelIndex  MI_CAR_RUINER2;
extern CModelIndex	MI_VAN_BOXVILLE5;

extern CModelIndex	MI_TRIKE_CHIMERA;
extern CModelIndex	MI_TRIKE_RROCKET;
extern CModelIndex	MI_QUADBIKE_BLAZER4;
extern CModelIndex	MI_CAR_RAPTOR;
extern CModelIndex	MI_BIKE_FAGGIO3;

extern CModelIndex	MI_CAR_TECHNICAL2;
extern CModelIndex	MI_CAR_TECHNICAL3;
extern CModelIndex	MI_CAR_DUNE3;
extern CModelIndex	MI_CAR_DUNE4;
extern CModelIndex	MI_CAR_DUNE5;
extern CModelIndex	MI_CAR_PHANTOM2;
extern CModelIndex	MI_CAR_WASTELANDER;
extern CModelIndex	MI_CAR_WASTELANDER2;
extern CModelIndex	MI_CAR_NERO;
extern CModelIndex	MI_CAR_GP1;
extern CModelIndex	MI_CAR_INFERNUS2;
extern CModelIndex	MI_CAR_RUSTON;
extern CModelIndex  MI_CAR_TAMPA3;
extern CModelIndex	MI_CAR_NIGHTSHARK;
extern CModelIndex	MI_CAR_PHANTOM3;
extern CModelIndex	MI_CAR_HAULER2;

extern CModelIndex	MI_CAR_DELUXO;
extern CModelIndex	MI_CAR_STROMBERG;
extern CModelIndex	MI_HELI_AKULA;
extern CModelIndex	MI_CAR_SENTINEL3;
extern CModelIndex	MI_CAR_BARRAGE;
extern CModelIndex	MI_PLANE_VOLATOL;
extern CModelIndex  MI_CAR_COMET4;
extern CModelIndex  MI_CAR_RAIDEN;
extern CModelIndex  MI_CAR_NEON;
extern CModelIndex  MI_CAR_Z190;
extern CModelIndex  MI_CAR_VISERIS;

extern CModelIndex	MI_CAR_TEZERACT;
extern CModelIndex	MI_HELI_SEASPARROW;

extern CModelIndex	MI_CAR_HALFTRACK;
extern CModelIndex	MI_CAR_APC;
extern CModelIndex	MI_CAR_XA21;
extern CModelIndex	MI_CAR_TORERO;
extern CModelIndex  MI_CAR_INSURGENT2;
extern CModelIndex	MI_CAR_INSURGENT3;
extern CModelIndex	MI_CAR_VAGNER;

extern CModelIndex	MI_CAR_PROTO;
extern CModelIndex	MI_CAR_VISIONE;

extern CModelIndex	MI_CAR_VIGILANTE;

// Heist3
extern CModelIndex MI_CAR_FURIA;
extern CModelIndex MI_CAR_STRYDER;
extern CModelIndex MI_CAR_ZHABA;
extern CModelIndex MI_CAR_MINITANK;
extern CModelIndex	MI_CAR_FORMULA;
extern CModelIndex	MI_CAR_FORMULA2;

// CnC
extern CModelIndex	MI_CAR_OPENWHEEL1;
extern CModelIndex	MI_CAR_OPENWHEEL2;

// Heist4
extern CModelIndex	MI_CAR_BRIOSO2;
extern CModelIndex	MI_CAR_TOREADOR;
extern CModelIndex	MI_SUB_KOSATKA;
extern CModelIndex	MI_CAR_ITALIRSX;
extern CModelIndex	MI_SUB_AVISA;
extern CModelIndex	MI_HELI_ANNIHILATOR2;
extern CModelIndex	MI_BOAT_PATROLBOAT;
extern CModelIndex	MI_HELI_SEASPARROW2;
extern CModelIndex	MI_HELI_SEASPARROW3;
extern CModelIndex	MI_CAR_VETO;
extern CModelIndex	MI_CAR_VETO2;
extern CModelIndex	MI_PLANE_ALKONOST;
extern CModelIndex	MI_BOAT_DINGHY5;
extern CModelIndex	MI_CAR_SLAMTRUCK;

// Tuner
extern CModelIndex	MI_CAR_CALICO;
extern CModelIndex	MI_CAR_COMET6;
extern CModelIndex  MI_CAR_CYPHER;
extern CModelIndex  MI_CAR_DOMINATOR7;
extern CModelIndex  MI_CAR_DOMINATOR8;
extern CModelIndex  MI_CAR_EUROS;
extern CModelIndex	MI_CAR_FREIGHTCAR2;
extern CModelIndex  MI_CAR_FUTO2;
extern CModelIndex  MI_CAR_GROWLER;
extern CModelIndex  MI_CAR_JESTER4;
extern CModelIndex  MI_CAR_PREVION;
extern CModelIndex  MI_CAR_REMUS;
extern CModelIndex  MI_CAR_RT3000;
extern CModelIndex  MI_CAR_SULTAN3;
extern CModelIndex  MI_CAR_TAILGATER2;
extern CModelIndex  MI_CAR_VECTRE;
extern CModelIndex  MI_CAR_WARRENER2;
extern CModelIndex  MI_CAR_ZR350;

//Fixer
extern CModelIndex	MI_CAR_ASTRON;
extern CModelIndex	MI_CAR_BALLER7;
extern CModelIndex	MI_CAR_BUFFALO4;
extern CModelIndex	MI_CAR_CHAMPION;
extern CModelIndex	MI_CAR_CINQUEMILA;
extern CModelIndex	MI_CAR_COMET7;
extern CModelIndex	MI_CAR_DEITY;
extern CModelIndex	MI_CAR_GRANGER2;
extern CModelIndex	MI_CAR_IGNUS;
extern CModelIndex	MI_CAR_IWAGEN;
extern CModelIndex	MI_CAR_JUBILEE;
extern CModelIndex  MI_CAR_PATRIOT3;
extern CModelIndex  MI_CAR_REEVER;
extern CModelIndex  MI_CAR_SHINOBI;
extern CModelIndex  MI_CAR_ZENO;
extern CModelIndex  MI_CAR_TENF;
extern CModelIndex  MI_CAR_TENF2;
extern CModelIndex	MI_CAR_TORERO2;
extern CModelIndex  MI_CAR_OMNISEGT;

//Gen9
extern CModelIndex MI_CAR_IGNUS2;

//HSW Upgrades
extern CModelIndex	MI_CAR_DEVESTE;
extern CModelIndex	MI_CAR_BRIOSO;
extern CModelIndex	MI_CAR_TURISMO2;
extern CModelIndex	MI_BIKE_HAKUCHOU2;

//tow truck hook
extern CModelIndex  MI_PROP_HOOK;

extern CModelIndex	MI_HELI_POLICE_1;
extern CModelIndex	MI_HELI_POLICE_2;
extern CModelIndex	MI_CAR_CABLECAR;

extern CModelIndex	MI_BOAT_POLICE;
extern CModelIndex	MI_BOAT_TUG;
extern CModelIndex	MI_BOAT_SEASHARK;
extern CModelIndex	MI_BOAT_SEASHARK2;
extern CModelIndex	MI_BOAT_SEASHARK3;

extern CModelIndex	MI_BOAT_TROPIC;
extern CModelIndex	MI_BOAT_TROPIC2;
extern CModelIndex	MI_BOAT_SPEEDER;
extern CModelIndex	MI_BOAT_SPEEDER2;
extern CModelIndex	MI_BOAT_TORO;
extern CModelIndex	MI_BOAT_TORO2;

// Pallet prop which is designed to work with the forklift.
extern CModelIndex	MI_FORKLIFT_PALLET;
// Shipping container designed to work with the Handler.
extern CModelIndex MI_HANDLER_CONTAINER;
// Digger with articulated arm
extern CModelIndex MI_DIGGER;

extern CModelIndex MI_BLIMP_3;

// Peds:
extern CModelIndex MI_PED_SLOD_HUMAN;
extern CModelIndex MI_PED_SLOD_SMALL_QUADPED;
extern CModelIndex MI_PED_SLOD_LARGE_QUADPED;
extern CModelIndex MI_PED_COP;
extern CModelIndex MI_PED_ORLEANS;

// Animal models
extern CModelIndex MI_PED_ANIMAL_COW;
extern CModelIndex MI_PED_ANIMAL_DEER;

// Player models
extern CModelIndex MI_PLAYERPED_PLAYER_ZERO;
extern CModelIndex MI_PLAYERPED_PLAYER_ONE;
extern CModelIndex MI_PLAYERPED_PLAYER_TWO;

// Parachute
extern CModelIndex MI_PED_PARACHUTE;

// Problematic objects which an AI ped might become stuck inside
extern CModelIndex MI_PROP_BIN_14A;

// Problematic fragable objects which player has difficultly with cover
extern CModelIndex MI_PROP_BOX_WOOD04A;

// For Sale signs used by the property exterior script in MP
extern CModelIndex MI_PROP_FORSALE_DYN_01;
extern CModelIndex MI_PROP_FORSALE_DYN_02;

extern CModelIndex MI_PROP_STUNT_TUBE_01;
extern CModelIndex MI_PROP_STUNT_TUBE_02;
extern CModelIndex MI_PROP_STUNT_TUBE_03;
extern CModelIndex MI_PROP_STUNT_TUBE_04;
extern CModelIndex MI_PROP_STUNT_TUBE_05;

extern CModelIndex MI_PROP_FOOTBALL;
extern CModelIndex MI_PROP_STUNT_FOOTBALL;
extern CModelIndex MI_PROP_STUNT_FOOTBALL2;


//
//
// model name hashes as simple constants:
//
enum {
MID_AKULA						= 0x46699F47, // "akula"
MID_ARDENT						= 0x097E5533, // "ardent"
MID_ASBO						= 0x42ACA95F, // "asbo"
MID_AUTARCH						= 0xED552C74, // "autarch"

MID_BARRAGE						= 0xF34DFB25, // "barrage"
MID_BALLER7						= 0x1573422D, // "baller7"
MID_BLAZER4						= 0xE5BA6858, // "blazer4"
MID_BLAZER5						= 0xA1355F67, // "blazer5"
MID_BODHI2						= 0xAA699BB6, // "bodhi2"
MID_BOMBUSHKA					= 0xFE0A508C, // "bombushka"
MID_BRIOSO2						= 0x55365079, // "brioso2"
MID_BRIOSO3						= 0x00E827DE, // "brioso3"
MID_BRUISER						= 0x27D79225, // "bruiser"
MID_BRUISER2					= 0x9B065C9E, // "bruiser2"
MID_BRUISER3					= 0x8644331A, // "bruiser3"
MID_BRUTUS						= 0x7F81A829, // "brutus"
MID_BRUTUS2						= 0x8F49AE28, // "brutus2"
MID_BRUTUS3						= 0x798682A2, // "brutus3"
MID_BTYPE2						= 0xCE6B35A4, // "btype2"
MID_BUCCANEER2					= 0xC397F748, // "buccaneer2"
MID_BUFFALO4					= 0xDB0C9B04, // "buffalo4"
MID_BULLDOZER					= 0x7074F39D, // "bulldozer"

MID_CALICO						= 0xB8D657AD, // "calico"
MID_CERBERUS					= 0xD039510B, // "cerberus"
MID_CERBERUS2					= 0x287FA449, // "cerberus2"
MID_CERBERUS3					= 0x71D3B6F0, // "cerberus3"
MID_CHAMPION					= 0xC972A155, // "champion"
MID_CHEETAH2					= 0x0D4E5F4D, // "cheetah2"
MID_CHERNOBOG					= 0xD6BC7523, // "chernobog"
MID_CLIQUE						= 0xA29F78B0, // "clique"
MID_COMET4						= 0x5D1903F9, // "comet4"
MID_COMET6						= 0x991EFC04, // "comet6"
MID_COMET7						= 0x440851D8, // "comet7"
MID_COQUETTE4					= 0x98F65A5E, // "coquette4"
MID_CORSITA						= 0xD3046147, // "corsita"
MID_CYCLONE2					= 0x170341C2, // "cyclone2"
MID_CYPHER						= 0x68A5D1EF, // "cypher"

MID_DEATHBIKE					= 0xFE5F0722, // "deathbike"
MID_DEATHBIKE2					= 0x93F09558, // "deathbike2"
MID_DEATHBIKE3					= 0xAE12C99C, // "deathbike3"
MID_DEITY						= 0x5B531351, // "deity"
MID_DELUXO						= 0x586765FB, // "deluxo"
MID_DEVESTE						= 0x5EE005DA, // "deveste"
MID_DEVIANT						= 0x4C3FFF49, // "deviant"
MID_DOMINATOR3					= 0xC52C6B93, // "dominator3"
MID_DOMINATOR4					= 0xD6FB0F30, // "dominator4"
MID_DOMINATOR5					= 0xAE0A3D4F, // "dominator5"
MID_DOMINATOR6					= 0xB2E046FB, // "dominator6"
MID_DOMINATOR7					= 0x196F9418, // "dominator7" 
MID_DOMINATOR8					= 0x2BE8B90A, // "dominator8"
MID_DRAFTER						= 0x28EAB80F, // "drafter"
MID_DRAUGUR						= 0xD235A4A6, // "draugur"
MID_DUKES3						= 0x7F3415E3, // "dukes3"
MID_DUMP						= 0x810369E2, // "dump"
MID_DUMMY_MOD_FRAG_MODEL		= 0x8941AF77, // special case for B*3818017 (FAILED: Streamed frag request for vehicle is not found : dummy_mod_frag_model);
MID_DUNE4						= 0xCEB28249, // "dune4"
MID_DUNE5						= 0xED62BFA9, // "dune5"
MID_DYNASTY						= 0x127E90D5, // "dynasty"

MID_ELEGY						= 0x0BBA2261, // "elegy"
MID_ELLIE						= 0xB472D2B5, // "ellie"
MID_EMERUS						= 0x4EE74355, // "emerus"
MID_EUROS						= 0x7980BDD5, // "euros"

MID_FAGALOA						= 0x6068AD86, // "fagaloa"
MID_FELTZER3					= 0xA29D6D10, // "feltzer3"
MID_FLASHGT						= 0xB4F32118, // "flashgt"
MID_FMJ							= 0x5502626C, // "FMJ"
MID_FORMULA						= 0x1446590A, // "formula"
MID_FURIA						= 0x3944D5A0, // "furia"
MID_FUTO2						= 0xA6297CC8, // "futo2"

MID_GAUNTLET3					= 0x2B0C4DCD, // "gauntlet3"
MID_GAUNTLET4					= 0x734C5E50, // "gauntlet4"
MID_GAUNTLET5					= 0x817AFAAD, // "gauntlet5"
MID_GB200						= 0x71CBEA98, // "gb200"
MID_GLENDALE2					= 0xC98BBAD6, // "glendale2"
MID_GP1							= 0x4992196C, // "gp1"
MID_GRANGER						= 0x9628879C, // "granger"
MID_GRANGER2					= 0xF06C29C7, // "granger2"
MID_GREENWOOD					= 0x026ED430, // "greenwood"
MID_GROWLER						= 0x4DC079D7, // "growler"

MID_HELLION						= 0xEA6A047F, // "hellion"
MID_HOTRING						= 0x42836BE5, // "hotring"
MID_HUSTLER						= 0x23CA25F2, // "hustler"

MID_TENF						= 0xCAB6E261, // "tenf"
MID_TENF2						= 0x10635A0E, // "tenf2"
MID_IGNUS						= 0xA9EC907B, // "ignus"
MID_IGNUS2						= 0x39085F47, // "ignus2"
MID_IMORGON						= 0xBC7C0A00, // "imorgon"
MID_IMPALER						= 0x83070B62, // "impaler"
MID_IMPALER2					= 0x3C26BD0C, // "impaler2"
MID_IMPALER3					= 0x8D45DF49, // "impaler3"
MID_IMPALER4					= 0x9804F4C7, // "impaler4"
MID_IMPERATOR					= 0x1A861243, // "imperator"
MID_IMPERATOR2					= 0x619C1B82, // "imperator2"
MID_IMPERATOR3					= 0xD2F77E37, // "imperator3"
MID_INFERNUS2					= 0xAC33179C, // "infernus2"
MID_ISSI3						= 0x378236E1, // "issi3"
MID_ISSI4						= 0x256E92BA, // "issi4"
MID_ISSI5						= 0x5BA0FF1E, // "issi5"
MID_ISSI6						= 0x49E25BA1, // "issi6"
MID_ISSI7						= 0x6E8DA4F7, // "issi7"
MID_ITALIGTO					= 0xEC3E3404, // "italigto"
MID_ITALIRSX					= 0xBB78956A, // "italirsx"

MID_JESTER3						= 0xF330CB6A, // "jester3"
MID_JESTER4						= 0xA1B3A871, // "jester4"
MID_JUBILEE						= 0x1B8165D3, // "jubilee"

MID_KALAHARI					= 0x05852838, // "kalahari"
MID_KANJOSJ						= 0xFC2E479A, // "kanjosj"
MID_KHANJALI					= 0xAA6F980A, // "khanjali"
MID_KOMODA						= 0xCE44C4B9, // "komoda"
MID_KRIEGER						= 0xD86A0247, // "krieger"
MID_KOSATKA						= 0x4FAF0D70, // "Kosatka"

MID_LONGFIN						= 0x6EF89CCC, // "longfin"

MID_MANANA2						= 0x665F785D, // "manana2"
MID_MANCHEZ2					= 0x40C332A3, // "manchez2"
MID_MENACER						= 0x79DD18AE, // "menacer"
MID_MICROLIGHT					= 0x96E24857, // "microlight"
MID_MONSTER						= 0xCD93A7DB, // "monster"
MID_MONSTER3					= 0x669EB40A, // "monster3"
MID_MONSTER4					= 0x32174AFC, // "monster4"
MID_MONSTER5					= 0xD556917C, // "monster5"
MID_MOONBEAM2					= 0x710A2B9B, // "moonbeam2"
MID_MOWER						= 0x6A4BD8F6, // "mower"
MID_MULE4						= 0x73F4110E, // "mule4"

MID_NEBULA						= 0xCB642637, // "nebula"
MID_NEO							= 0x9F6ED5A2, // "neo"
MID_NEON						= 0x91CA96EE, // "neon"
MID_NERO2						= 0x4131f378, // "nero2"

MID_OPENWHEEL1					= 0x58F77553, // "openwheel1"
MID_OPENWHEEL2					= 0x4669D038, // "openwheel2"
MID_OPPRESSOR					= 0x34B82784, // "oppressor"
MID_OPPRESSOR2					= 0x7B54A9D3, // "oppressor2"

MID_PATRIOT2					= 0xE6E967F8, // "patriot2"
MID_PATRIOT3					= 0xD80F4A44, // "patriot3"
MID_PARAGON						= 0xE550775B, // "paragon"
MID_PBUS2						= 0x149BD32A, // "pbus2"
MID_PENETRATOR					= 0x9734F3EA, // "penetrator"
MID_PENUMBRA2					= 0xDA5EC7DA, // "penumbra2"
MID_PEYOTE2						= 0x9472CD24, // "peyote2"
MID_PEYOTE3						= 0x4201A843, // "peyote3"
MID_PHANTOM2					= 0x9dae1398, // "phantom2"
MID_POLBATI						= 0x00EB2195, // "polbati"
MID_POLBUFFALO					= 0x11291A17, // "polbuffalo"
MID_POLCARACARA					= 0x8BD565B8, // "polcaracara"
MID_POLGAUNTLET					= 0xB67633E6, // "polgauntlet"
MID_POLGRANGER					= 0x7DFAB6F9, // "polgranger"
MID_POLGREENWOOD				= 0x678DD3EA, // "polgreenwood"
MID_POLICE						= 0x79FBB0C5, // "police"
MID_POLICE2						= 0x9F05F101, // "police2"
MID_POLICE3						= 0x71FA16EA, // "police3"
MID_POLICE4						= 0x8A63C7B9, // "police4" 
MID_POLICE5						= 0x9C32EB57, // "police5"
MID_POLICE6						= 0xB2FF98F0, // "police6"
MID_POLICEB2					= 0x8D780D37, // "policeb2"
MID_POLICET2					= 0x295DBE34, // "policet2"
MID_POLPANTO					= 0xB0BCAB81, // "polpanto"
MID_POLRIATA					= 0x20A4CEF4, // "polriata"
MID_POUNDER2					= 0x6290F15B, // "pounder2"
MID_PREVION						= 0x546DA331, // "previon"
MID_PRIMO3						= 0xB81AF24C, // "primo3"
MID_PROTOTIPO					= 0x7E8F677F, // "prototipo"

MID_RAPIDGT3					= 0x7A2EF5E4, // "rapidgt3"
MID_RAPTOR						= 0xD7C56D39, // "raptor"
MID_RCBANDITO					= 0xEEF345EC, // "rcbandito"
MID_REBLA						= 0x04F48FC4, // "rebla"
MID_REMUS						= 0x5216AD5E, // "remus"
MID_RHINO						= 0x2EA68690, // "rhino"
MID_RIOT						= 0xB822A1AA, // "riot"
MID_RIOT2						= 0x9B16A3B4, // "riot2"
MID_RROCKET						= 0x36A167E0, // "rrocket"
MID_RT3000						= 0xE505CF99, // "rt3000"
MID_RUINER2						= 0x381E10BD, // "ruiner2"
MID_RUINER4						= 0x65BDEBFC, // "ruiner4"
MID_RUSTON						= 0x2ae524a8, // "ruston"

MID_S80							= 0xECA6B6A3, // "s80"
MID_SAVESTRA					= 0x35DED0DD, // "savestra"
MID_SC1							= 0x5097F589, // "sc1"
MID_SCARAB						= 0xBBA2A2F7, // "scarab"
MID_SCARAB2						= 0x5BEB3CE0, // "scarab2"
MID_SCARAB3						= 0xDD71BFEB, // "scarab3"
MID_SCHLAGEN					= 0xE1C03AB0, // "schlagen"
MID_SEASPARROW2					= 0x494752F7, // "seasparrow2"
MID_SENTINEL4					= 0xAF1FA439, // "sentinel4"
MID_SHERIFF						= 0x9BAA707C, // "sheriff"
MID_SHOTARO						= 0xE7D2A16E, // "shotaro"
MID_SLAMTRUCK					= 0xC1A8A914, // "slamtruck"
MID_SLAMVAN4					= 0x8526E2F5, // "slamvan4"
MID_SLAMVAN5					= 0x163F8520, // "slamvan5"
MID_SLAMVAN6					= 0x67D52852, // "slamvan6"
MID_SM722						= 0x2E3967B0, // "sm722"
MID_STAFFORD					= 0x1324E960, // "stafford"
MID_STROMBERG					= 0x34DBA661, // "stromberg"
MID_STRYDER						= 0x11F58A5A, // "stryder"
MID_SUGOI						= 0x3ADB9758, // "sugoi"
MID_SULTAN3						= 0xEEA75E63, // "sultan3"
MID_SULTANRS					= 0xEE6024BC, // "sultanrs"
MID_SWINGER						= 0x1DD4C0FF, // "swinger"

MID_T20							= 0x6322B39A, // "t20"
MID_TAILGATER2					= 0xb5D306A4, // "tailgater2"
MID_TAMPA3						= 0xB7D9F7F1, // "tampa3"
MID_THRAX						= 0x3E3D1F59, // "thrax"
MID_THRUSTER					= 0x58CDAF30, // "thruster"
MID_TIGON						= 0xAF0B8D48, // "tigon"
MID_TOREADOR					= 0x56C8A5EF, // "toreador"
MID_TORERO2						= 0xF62446BA, // "torero2"
MID_TORNADO6					= 0xA31CB573, // "tornado6"
MID_TRACTOR						= 0x61D6BA8C, // "tractor"
MID_TRACTOR2					= 0x843B73DE, // "tractor2"
MID_TRAILERS					= 0xCBB2BE0E, // "trailers"
MID_TRAILERS2					= 0xA1DA3C91, // "trailers2"
MID_TRAILERS3					= 0x8548036D, // "trailers3"
MID_TRAILERLOGS					= 0x782A236D, // "trailerlogs"
MID_TULIP						= 0x56D42971, // "tulip"
MID_TYRANT						= 0xE99011C2, // "tyrant"

MID_VAGNER						= 0x7397224C, // "vagner"
MID_VAGRANT						= 0x2C1FEA99, // "vagrant"
MID_VAMOS						= 0xFD128DFD, // "vamos"
MID_VECTRE						= 0xA42FC3A5, // "vectre"
MID_VERUS						= 0x11CBC051, // "verus"
MID_VETO2						= 0xA703E4A9, // "veto2"
MID_VIGILANTE					= 0xB5EF4C33, // "vigilante"
MID_VISERIS						= 0xE8A8BA94, // "viseris"
MID_VISIONE						= 0xC4810400, // "visione"
MID_VOLATOL						= 0x1AAD0DED, // "volatol"
MID_VOLTIC2						= 0x3AF76F4A, // "voltic2"
MID_VOODOO						= 0x779B4F2D, // "VOODOO"
MID_VSTR						= 0x56CDEE7D, // "vstr"

MID_WARRENER2					= 0x2290C50A, // "warrener2"
MID_WEEVIL						= 0x61FE4D6A, // "weevil"
MID_WEEVIL2						= 0xC4BB1908, // "weevil2"
MID_WINKY						= 0xF376F1E6, // "winky"

MID_XA21						= 0x36B4A8A9, // "xa21"

MID_YOUGA3						= 0x6B73A9BE, // "youga3"
MID_YOUGA4						= 0x589A840C, // "youga4"
MID_YOSEMITE2					= 0x64F49967, // "yosemite2"
MID_YOSEMITE3					= 0x0409D787, // "yosemite3"

MID_Z190						= 0x3201DD49, // "z190"
MID_ZENTORNO					= 0xAC5DF515, // "zentorno"
MID_ZENO						= 0x2714AA93, // "zeno"
MID_ZION3						= 0x6F039A67, // "zion3"
MID_ZORRUSSO					= 0xD757D97D, // "zorrusso"
MID_ZR350						= 0x91373058, // "zr350"
MID_ZR380						= 0x20314B42, // "zr380"
MID_ZR3802						= 0xBE11EFC6, // "zr3802"
MID_ZR3803						= 0xA7DCC35C  // "zr3803"
};


// [HACK GTAV]
// parachutes and parachute bags
enum {
MID_P_PARACHUTE1_S				= 0xED4D99B5,	// "p_parachute1_s"
MID_P_PARACHUTE1_SP_S			= 0x0D06C8DF,	// "p_parachute1_SP_s"
MID_P_PARACHUTE1_MP_S			= 0x4FAA899A,	// "p_parachute1_MP_s"
MID_PIL_P_PARA_PILOT_SP_S		= 0x815E52EB,	// "pil_p_para_pilot_sp_s"
MID_LTS_P_PARA_PILOT2_SP_S		= 0x73268708,	// "lts_p_para_pilot2_sp_s"
MID_SR_PROP_SPECRACES_PARA_S_01 = 0x0DB6AD2D,	// "sr_prop_specraces_para_s_01"
MID_GR_PROP_GR_PARA_S_01		= 0xCC7BF93A,	// "gr_prop_gr_para_s_01"
MID_XM_PROP_X17_PARA_SP_S		= 0x95DE0B0C,	// "xm_prop_x17_para_sp_s"
MID_TR_PROP_TR_PARA_SP_S_01A	= 0x0032993B,	// "tr_prop_tr_para_sp_s_01a"
MID_REH_PROP_REH_PARA_SP_S_01A	= 0xB17BB174,	// "reh_prop_reh_para_sp_s_01a"

MID_PIL_P_PARA_BAG_PILOT_S		= 0x582DCF54,	// "pil_p_para_bag_pilot_s"
MID_LTS_P_PARA_BAG_LTS_S		= 0x4C28BD84,	// "lts_P_para_bag_LTS_S"
MID_LTS_P_PARA_BAG_PILOT2_S		= 0x4BAA1F65,	// "lts_P_para_bag_Pilot2_S"
MID_P_PARA_BAG_XMAS_S			= 0x694D27C4,	// "P_para_bag_Xmas_S"
MID_VW_P_PARA_BAG_VINE_S		= 0x93321CAD,	// "vw_p_para_bag_vine_s"
MID_P_PARA_BAG_TR_S_01A			= 0x691353BB,	// "p_para_bag_tr_s_01a"
MID_TR_P_PARA_BAG_TR_S_01A		= 0x95455AB8,	// "tr_p_para_bag_tr_s_01a"
MID_REH_P_PARA_BAG_REH_S_01A	= 0xA027FCDB	// "reh_p_para_bag_reh_s_01a"
};


// GTAV DLC Interiors:
enum {
MID_INT_MP_H_01					= 0x1D203ABF,	// "int_mp_h_01"
MID_INT_MP_M_01					= 0x46EE9764,	// "int_mp_m_01"
MID_INT_MP_L_01					= 0x7489890E,	// "int_mp_l_01"
MID_INT_MP_H_02					= 0x53E32844,	// "int_mp_h_02"
MID_INT_MP_M_02					= 0x511AABBC,	// "int_mp_m_02"
MID_INT_MP_L_02					= 0x86252C45,	// "int_mp_l_02"
MID_INT_MP_H_03					= 0x46B08DDF,	// "int_mp_h_03"
MID_INT_MP_M_03					= 0xAA80DE8B,	// "int_mp_m_03"
MID_INT_MP_L_03					= 0x986C50D3,	// "int_mp_l_03"
MID_INT_MP_H_04					= 0x798DF399,	// "int_mp_h_04"
MID_INT_MP_M_04					= 0xB455F235,	// "int_mp_m_04"
MID_INT_MP_L_04					= 0xAC7178DD,	// "int_mp_l_04"
MID_INT_MP_H_05					= 0x6B545726,	// "int_mp_h_05"
MID_INT_MP_M_05					= 0xED72E46E,	// "int_mp_m_05"
MID_INT_MP_L_05					= 0xBD8F1B18	// "int_mp_l_05"
};

#endif // _MODELINDICES_H_...
