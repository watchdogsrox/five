// Title	:	ModelIndices.h
// Author	:	ObR
// Started	:	29/06/00
// Stuff to fill in the modelindices values. Do loads of strcmps to
// find objects with a specific name. This way we don't have a problem
// if numbers change.

#include "modelindices.h"
#include "modelinfo/ModelInfo.h"

#if __WIN32
#pragma warning(push)
#pragma warning(disable: 4073) // initializers put in library initialization area
#pragma init_seg(lib)
#pragma warning(pop)
#endif
CModelIndex* CModelIndex::sm_pFirst=NULL;
#if __ASSERT
bool CModelIndex::sm_matchedStrings = false;
#endif

// Do this before computing any hashes during static initialization
static atHashStringNamespaceSupport::EarlyInit g_InitNamespacesEarly;


CModelIndex::CModelIndex(const strStreamingObjectName& pName)
{
	Assertf(sm_matchedStrings == false, "Cannot create a CModelIndex once all the strings have been matched");

	m_pName = pName;
	m_index = fwModelId::MI_INVALID;
//	m_modelId.Invalidate();
	m_pNext = sm_pFirst;
	sm_pFirst = this;
}

void CModelIndex::MatchAllModelStrings()
{
	CModelIndex* pModelIndex = sm_pFirst;
	while(pModelIndex)
	{
		pModelIndex->m_index = fwModelId::MI_INVALID;	// Obbe: Need this for the episodes. Otherwise an object that was found in e1 will still have a modelindex even if gta4 was restarted and the model doesn't exist in gta4 (happened with seagull)
//		pModelIndex->m_modelId.Invalidate();

		//CModelInfo::GetModelInfo_2Arg(pModelIndex->m_pName, &pModelIndex->m_index );
		fwModelId modelId = CModelInfo::GetModelIdFromName(pModelIndex->m_pName);
		pModelIndex->m_index = modelId.GetModelIndex();

		if(!modelId.IsValid())
		{
#ifndef NM_BUILD // This warning can spew out the names of peds which we exclude from builds given to Natural Motion.
			Warningf("%s doesn't exist", pModelIndex->m_pName.TryGetCStr());
#endif // NM_BUILD
		}
		pModelIndex = pModelIndex->m_pNext;
	}
#if __ASSERT
	sm_matchedStrings = true;
#endif
}

CModelIndex	MI_CAR_CABLECAR(strStreamingObjectName("cablecar",0xC6C3242D));//not in .meta
CModelIndex	MI_BOAT_POLICE(strStreamingObjectName("predator",0xE2E7D4AB));//not in .meta
CModelIndex	MI_BOAT_TUG( strStreamingObjectName( "tug", 0x82CAC433 ) );
CModelIndex	MI_BOAT_TROPIC( strStreamingObjectName( "TROPIC", 0x1149422F ) );
CModelIndex	MI_BOAT_TROPIC2( strStreamingObjectName( "tropic2", 0x56590FE9 ) );
CModelIndex	MI_BOAT_SPEEDER( strStreamingObjectName( "speeder", 0x0dc60d2b ) );
CModelIndex	MI_BOAT_SPEEDER2( strStreamingObjectName( "speeder2", 0x1A144F2A ) );
CModelIndex	MI_BOAT_TORO( strStreamingObjectName( "toro", 0x3fd5aa2f ) );
CModelIndex	MI_BOAT_TORO2( strStreamingObjectName( "toro2", 0x362CAC6D ) );
CModelIndex	MI_BOAT_SEASHARK( strStreamingObjectName( "seashark", 0xc2974024 ) );
CModelIndex	MI_BOAT_SEASHARK2( strStreamingObjectName( "seashark2", 0xdb4388e4 ) );
CModelIndex	MI_BOAT_SEASHARK3( strStreamingObjectName( "seashark3", 0xED762D49 ) );


// Objects:
CModelIndex MI_CELLPHONE(strStreamingObjectName("Prop_Player_Phone_01",0x2D5AF569));
CModelIndex MI_PROP_HOOK(strStreamingObjectName("Prop_V_Hook_S",0xBC344305));
CModelIndex MI_FORKLIFT_PALLET(strStreamingObjectName("p_pallet_02a_s",0xB191A903));
CModelIndex MI_HANDLER_CONTAINER(strStreamingObjectName("Prop_Contr_03b_LD",0x342160A2));
CModelIndex MI_DIGGER(strStreamingObjectName("digger",0xBEE388D));
CModelIndex MI_BLIMP_3(strStreamingObjectName("blimp3",0xEDA4ED97));

CModelIndex MI_WINDMILL(strStreamingObjectName("prop_windmill_01", 0x745f3383));

CModelIndex MI_PHONEBOX_4(strStreamingObjectName("prop_phonebox_04", 0x451454d2));

CModelIndex MI_LRG_GATE_L(strStreamingObjectName("prop_lrggate_01_l", 0xEB278B23));
CModelIndex MI_LRG_GATE_R(strStreamingObjectName("prop_lrggate_01_r", 0x8DA65022));

CModelIndex MI_GROUND_LIGHT_RED(strStreamingObjectName("prop_runlight_r",0x2CFD9427));
CModelIndex MI_GROUND_LIGHT_GREEN(strStreamingObjectName("prop_runlight_g",0x8644AE1));
CModelIndex MI_GROUND_LIGHT_YELLOW(strStreamingObjectName("prop_runlight_y",0xAD751518));

// Railway crossing barriers:
CModelIndex MI_RAILWAY_CROSSING_BARRIER_1(strStreamingObjectName("prop_traffic_rail_1b",0xDB147CA8));
CModelIndex MI_RAILWAY_BARRIER_01(strStreamingObjectName("prop_railway_barrier_01",0xd639398a));

// Props with env cloth, requiring avoiding
CModelIndex MI_PROP_FNCLINK_04J(strStreamingObjectName("prop_fnclink_04j",0xe540f236));
CModelIndex MI_PROP_FNCLINK_04K(strStreamingObjectName("prop_fnclink_04k",0x7bf61fa2));
CModelIndex MI_PROP_FNCLINK_04L(strStreamingObjectName("prop_fnclink_04l",0xc1cb2b4b));
CModelIndex MI_PROP_FNCLINK_04M(strStreamingObjectName("prop_fnclink_04m",0xd08cc8ce));
CModelIndex MI_PROP_FNCLINK_02I(strStreamingObjectName("prop_fnclink_02i",0x66aa774f));
CModelIndex MI_PROP_FNCLINK_02J(strStreamingObjectName("prop_fnclink_02j",0x0f31c85f));
CModelIndex MI_PROP_FNCLINK_02K(strStreamingObjectName("prop_fnclink_02k",0x04deb3b9));
CModelIndex MI_PROP_FNCLINK_02L(strStreamingObjectName("prop_fnclink_02l",0xabad8154));
CModelIndex MI_PROP_AIR_WINDSOCK(strStreamingObjectName("prop_air_windsock",0xed79fddc));
CModelIndex MI_PROP_AIR_CARGO_01A(strStreamingObjectName("prop_air_cargo_01a",0x5ff8d96f));
CModelIndex MI_PROP_AIR_CARGO_01B(strStreamingObjectName("prop_air_cargo_01b",0x70ba7af2));
CModelIndex MI_PROP_AIR_CARGO_01C(strStreamingObjectName("prop_air_cargo_01c",0x3e591630));
CModelIndex MI_PROP_AIR_CARGO_02A(strStreamingObjectName("prop_air_cargo_02a",0xefaae110));
CModelIndex MI_PROP_AIR_CARGO_02B(strStreamingObjectName("prop_air_cargo_02b",0x205d4274));
CModelIndex MI_PROP_AIR_TRAILER_1A(strStreamingObjectName("prop_air_trailer_1a",0xe84cfc9f));
CModelIndex MI_PROP_AIR_TRAILER_1B(strStreamingObjectName("prop_air_trailer_1b",0xbabea183));
CModelIndex MI_PROP_AIR_TRAILER_1C(strStreamingObjectName("prop_air_trailer_1c",0x8b55c2c6));
CModelIndex MI_PROP_SKID_TENT_CLOTH(strStreamingObjectName("prop_skid_tent_cloth",0x24ca7804));
CModelIndex MI_PROP_SKID_TENT_01(strStreamingObjectName("prop_skid_tent_01",0xdc6b5d07));
CModelIndex MI_PROP_SKID_TENT_01B(strStreamingObjectName("prop_skid_tent_01b",0x69ef0318));
CModelIndex MI_PROP_SKID_TENT_03(strStreamingObjectName("prop_skid_tent_03",0xc114a65a));

CModelIndex MI_PROP_BEACH_PARASOL_01(strStreamingObjectName("prop_beach_parasol_01",0x3edc551b));
CModelIndex MI_PROP_BEACH_PARASOL_02(strStreamingObjectName("prop_beach_parasol_02",0x2d1cb19c));
CModelIndex MI_PROP_BEACH_PARASOL_03(strStreamingObjectName("prop_beach_parasol_03",0x1ecf1501));
CModelIndex MI_PROP_BEACH_PARASOL_04(strStreamingObjectName("prop_beach_parasol_04",0x8d027172));
CModelIndex MI_PROP_BEACH_PARASOL_05(strStreamingObjectName("prop_beach_parasol_05",0x82505c0e));
CModelIndex MI_PROP_BEACH_PARASOL_06(strStreamingObjectName("prop_beach_parasol_06",0x720dbb89));
CModelIndex MI_PROP_BEACH_PARASOL_07(strStreamingObjectName("prop_beach_parasol_07",0x57d48717));
CModelIndex MI_PROP_BEACH_PARASOL_08(strStreamingObjectName("prop_beach_parasol_08",0xc597e29c));
CModelIndex MI_PROP_BEACH_PARASOL_09(strStreamingObjectName("prop_beach_parasol_09",0xbb61ce30));
CModelIndex MI_PROP_BEACH_PARASOL_10(strStreamingObjectName("prop_beach_parasol_10",0x586c8f1b));

// Money bag used in Capture jobs:
CModelIndex MI_MONEY_BAG(strStreamingObjectName("prop_money_bag_01", 0x113FD533));

// Cars:
CModelIndex	MI_CAR_VOLTIC(strStreamingObjectName("VOLTIC",0x9F4B77BE));
CModelIndex	MI_CAR_VOLTIC2(strStreamingObjectName("VOLTIC2",0x3AF76F4A));
CModelIndex	MI_CAR_TORNADO(strStreamingObjectName("TORNADO",0x1BB290BC));
CModelIndex	MI_CAR_TORNADO6( strStreamingObjectName( "tornado6", 0xA31CB573 ) );
// law enforcement
CModelIndex	MI_CAR_CRUSADER(strStreamingObjectName("crusader",0x132D5A1A));
CModelIndex	MI_CAR_POLICE(strStreamingObjectName("police",0x79FBB0C5));
CModelIndex	MI_CAR_POLICE_2(strStreamingObjectName("police2",0x9F05F101));
CModelIndex MI_CAR_POLICE_3(strStreamingObjectName("police3",0x71fa16ea));
CModelIndex	MI_CAR_SHERIFF(strStreamingObjectName("sheriff",0x9BAA707C));
CModelIndex	MI_CAR_NOOSE(strStreamingObjectName("noosevan",0x3E1E987C));
CModelIndex MI_CAR_NOOSE_JEEP(strStreamingObjectName("fbi2",0x9DC66994));
CModelIndex MI_CAR_NOOSE_JEEP_2(strStreamingObjectName("sheriff2",0x72935408));
CModelIndex	MI_CAR_FBI(strStreamingObjectName("fbi",0x432EA949));
CModelIndex	MI_CAR_POLICE_CURRENT(strStreamingObjectName("police",0x79FBB0C5));
CModelIndex	MI_CAR_CLASSIC_TAXI(strStreamingObjectName("taxi",0xC703DB5F));
CModelIndex	MI_CAR_TAXI_CURRENT_1(strStreamingObjectName("empty",0xBBD57BED));
CModelIndex	MI_CAR_SECURICAR(strStreamingObjectName("stockade",0x6827CF72));
CModelIndex	MI_HELI_POLICE_1(strStreamingObjectName("polmav",0x1517D4D9));
CModelIndex	MI_HELI_POLICE_2(strStreamingObjectName("annihilator",0x31F0B376));
CModelIndex MI_VAN_RIOT(strStreamingObjectName("riot", 0xB822A1AA));
CModelIndex MI_VAN_RIOT_2(strStreamingObjectName("riot2", 0x9B16A3B4));

// other
CModelIndex	MI_CAR_AMBULANCE(strStreamingObjectName("ambulance",0x45D56ADA));
CModelIndex MI_CAR_FORKLIFT(strStreamingObjectName("forklift",0x58E49664));
CModelIndex MI_CAR_HANDLER(strStreamingObjectName("handler",0x1A7FCEFA));
CModelIndex	MI_CAR_FIRETRUCK(strStreamingObjectName("firetruk",0x73920F8E));
CModelIndex	MI_CAR_GARBAGE_COLLECTION(strStreamingObjectName("trash",0x72435A19));
CModelIndex MI_CAR_TAILGATER(strStreamingObjectName("tailgater",0xC3DDFDCE));
CModelIndex MI_CAR_TOWTRUCK(strStreamingObjectName("towtruck",0xB12314E0));
CModelIndex MI_CAR_TOWTRUCK_2(strStreamingObjectName("towtruck2",0xE5A2D6C6));
CModelIndex MI_TRAILER_TR2(strStreamingObjectName("tr2", 0x7BE032C6));
CModelIndex MI_TRAILER_TR4(strStreamingObjectName("tr4", 0x7CAB34D0));
CModelIndex MI_CAR_MONROE(strStreamingObjectName("monroe",0xE62B361B));
CModelIndex	MI_CAR_ZTYPE(strStreamingObjectName("ztype",0x2d3bd401));
CModelIndex MI_CAR_CADDY(strStreamingObjectName("caddy",0x44623884));
CModelIndex MI_CAR_CADDY3(strStreamingObjectName("caddy3",0xD227BDBB));
CModelIndex MI_CAR_HAULER(strStreamingObjectName("hauler",0x5a82f9ae));
CModelIndex MI_CAR_MESA3(strStreamingObjectName("MESA3",0x84f42e51));
CModelIndex MI_CAR_SPEEDO(strStreamingObjectName("speedo", 0xCFB3870C));
CModelIndex MI_CAR_SPEEDO2(strStreamingObjectName("speedo2", 0x2B6DC64A));
CModelIndex MI_CAR_BTYPE(strStreamingObjectName("btype", 0x6FF6914));
CModelIndex MI_CAR_BTYPE3(strStreamingObjectName("btype3", 0xDC19D101));
CModelIndex MI_CAR_ZENTORNO(strStreamingObjectName("zentorno", 0xAC5DF515));
CModelIndex MI_CAR_CYCLONE2(strStreamingObjectName("cyclone2", 0x170341C2));
CModelIndex MI_CAR_CORSITA(strStreamingObjectName("corsita", 0xD3046147));
CModelIndex MI_CAR_PANTO(strStreamingObjectName("panto", 0xe644e480));
CModelIndex MI_CAR_GLENDALE(strStreamingObjectName("glendale", 0x47A6BC1));
CModelIndex MI_CAR_PIGALLE(strStreamingObjectName("pigalle", 0x404B6381));
CModelIndex MI_CAR_COQUETTE2(strStreamingObjectName("coquette2", 0x3c4e2113));
CModelIndex MI_CAR_RATLOADER(strStreamingObjectName("ratloader", 0xd83c13ce));
CModelIndex MI_CAR_BUCCANEER2(strStreamingObjectName("BUCCANEER2", 0xc397f748));
CModelIndex MI_CAR_KURUMA2(strStreamingObjectName("KURUMA2", 0x187D938D));
CModelIndex MI_CAR_BRAWLER(strStreamingObjectName("BRAWLER", 0xa7ce1bc5));
CModelIndex MI_CAR_INSURGENT(strStreamingObjectName("INSURGENT", 0x9114EADA));
CModelIndex MI_CAR_LIMO2(strStreamingObjectName("LIMO2", 0xF92AEC4D));
CModelIndex MI_CAR_MAMBA(strStreamingObjectName("MAMBA", 0x9CFFFC56));
CModelIndex	MI_TRAILER_TRAILER(strStreamingObjectName("trailers",0xcbb2be0e));
CModelIndex MI_BIKE_SANCHEZ(strStreamingObjectName("sanchez",0x2ef89e46));
CModelIndex MI_BIKE_SANCHEZ2(strStreamingObjectName("sanchez2",0xa960b13e));
CModelIndex MI_BIKE_BMX(strStreamingObjectName("BMX",0x43779C54));
CModelIndex MI_BIKE_SCORCHER(strStreamingObjectName("scorcher",0xF4E1AA15));
CModelIndex MI_BIKE_THRUST(strStreamingObjectName("thrust",0x6d6f8f43));
CModelIndex MI_BIKE_BF400(strStreamingObjectName("bf400",0x5283265));
CModelIndex MI_BIKE_GARGOYLE(strStreamingObjectName("gargoyle",0x2C2C2324));
CModelIndex MI_BIKE_SHOTARO(strStreamingObjectName("shotaro",0xE7D2A16E));
CModelIndex MI_BIKE_SANCTUS(strStreamingObjectName("sanctus",0x58E316C7));
CModelIndex MI_BIKE_ESSKEY(strStreamingObjectName("esskey",0x794CB30C));
CModelIndex MI_BIKE_OPPRESSOR(strStreamingObjectName("oppressor",0x34B82784));
CModelIndex MI_HELI_CARGOBOB(strStreamingObjectName("cargobob",0xFCFCB68B));
CModelIndex MI_HELI_CARGOBOB2(strStreamingObjectName("cargobob2",0x60A7EA10));
CModelIndex MI_HELI_CARGOBOB3(strStreamingObjectName("cargobob3",0x53174EEF));
CModelIndex MI_HELI_CARGOBOB4(strStreamingObjectName("cargobob4",0x78BC1A3C));
CModelIndex MI_HELI_CARGOBOB5(strStreamingObjectName("cargobob5", 0xEAFA7EB7));
CModelIndex MI_HELI_DRONE(strStreamingObjectName("drone",0xBEE66AEB));
CModelIndex MI_HELI_DRONE_2(strStreamingObjectName("drone2",0x9D525227));
CModelIndex MI_HELI_VALKYRIE(strStreamingObjectName("valkyrie",0xA09E15FD));
CModelIndex MI_HELI_VALKYRIE2(strStreamingObjectName("valkyrie2",0x5BFA5C4B));
CModelIndex MI_HELI_SWIFT(strStreamingObjectName("SWIFT",0xEBC24DF2));
CModelIndex MI_HELI_SWIFT2(strStreamingObjectName("SWIFT2",0x4019cb4c));
CModelIndex MI_HELI_SAVAGE(strStreamingObjectName("SAVAGE",0xfb133a17));
CModelIndex MI_HELI_SUPER_VOLITO(strStreamingObjectName("SUPERVOLITO",0x2A54C47D));
CModelIndex MI_HELI_SUPER_VOLITO2(strStreamingObjectName("SUPERVOLITO2",0x9C5E5644));
CModelIndex MI_HELI_VOLATUS(strStreamingObjectName("VOLATUS",0x920016F1));
CModelIndex MI_CAR_PBUS(strStreamingObjectName("PBUS",0x885F3671));
CModelIndex MI_TANK_RHINO(strStreamingObjectName("RHINO",0x2EA68690));
CModelIndex MI_TANK_KHANJALI(strStreamingObjectName("khanjali", 0xaa6f980a));
CModelIndex MI_CAR_BENSON_TRUCK(strStreamingObjectName("benson",0x7A61B330));
CModelIndex MI_PROP_TRAILER(strStreamingObjectName("proptrailer",0x153E1B0A));
CModelIndex MI_PLANE_JET(strStreamingObjectName("jet",0x3F119114));
CModelIndex MI_PLANE_TITAN(strStreamingObjectName("titan",0x761e2ad3));
CModelIndex MI_PLANE_CARGOPLANE(strStreamingObjectName("cargoplane",0x15f27762));
CModelIndex MI_PLANE_MAMMATUS(strStreamingObjectName("mammatus",0x97e55d11));
CModelIndex MI_PLANE_DODO(strStreamingObjectName("dodo",0xca495705));
CModelIndex MI_PLANE_VESTRA(strStreamingObjectName("vestra",0x4ff77e37));
CModelIndex MI_PLANE_STUNT(strStreamingObjectName("stunt",0x81794c70));
CModelIndex MI_PLANE_LUXURY_JET(strStreamingObjectName("luxor",0x250B0C5E));
CModelIndex MI_PLANE_LUXURY_JET2(strStreamingObjectName("shamal",0xB79C1BF5));
CModelIndex MI_PLANE_LUXURY_JET3(strStreamingObjectName("luxor2",0xb79f589e));
CModelIndex MI_PLANE_DUSTER(strStreamingObjectName("duster",0x39d6779e));
CModelIndex MI_PLANE_VELUM(strStreamingObjectName("velum",0x9C429B6A));
CModelIndex MI_PLANE_VELUM2(strStreamingObjectName("velum2",0x403820E8));
CModelIndex MI_PLANE_BESRA(strStreamingObjectName("besra",0x6CBD1D6D));
CModelIndex MI_PLANE_MILJET(strStreamingObjectName("miljet",0x09d80f93));
CModelIndex MI_PLANE_CUBAN(strStreamingObjectName("cuban800",0xd9927fe3));
CModelIndex MI_PLANE_HYDRA(strStreamingObjectName("hydra",0x39d6e83f));
CModelIndex MI_PLANE_NIMBUS(strStreamingObjectName("nimbus",0xB2CF7250));

CModelIndex MI_JETPACK_THRUSTER(strStreamingObjectName("thruster",0x58CDAF30));

CModelIndex MI_PLANE_TULA(strStreamingObjectName("tula",0x3E2E4F8A));
CModelIndex MI_PLANE_MICROLIGHT(strStreamingObjectName("microlight",0x96E24857));
CModelIndex MI_PLANE_BOMBUSHKA(strStreamingObjectName("bombushka",0xFE0A508C));
CModelIndex MI_PLANE_MOGUL(strStreamingObjectName("mogul",0xD35698EF));
CModelIndex MI_PLANE_STARLING(strStreamingObjectName("starling",0x9A9EB7DE));
CModelIndex MI_PLANE_PYRO(strStreamingObjectName("pyro",0xAD6065C0));
CModelIndex MI_PLANE_NOKOTA(strStreamingObjectName("nokota",0x3DC92356));
CModelIndex MI_PLANE_HOWARD(strStreamingObjectName("howard",0xC3F25753));
CModelIndex MI_PLANE_ROGUE(strStreamingObjectName("rogue",0xC5DD6967));
CModelIndex MI_PLANE_MOLOTOK(strStreamingObjectName("molotok",0x5D56F01B));
CModelIndex MI_PLANE_SEABREEZE(strStreamingObjectName("seabreeze",0xE8983F9F));

CModelIndex MI_PLANE_AVENGER(strStreamingObjectName("avenger",0x81BD2ED0));

CModelIndex MI_HELI_HAVOK(strStreamingObjectName("havok",0x89ba59f5));
CModelIndex MI_HELI_HUNTER(strStreamingObjectName("hunter",0xFD707EDE));

CModelIndex MI_HELI_BUZZARD(strStreamingObjectName("buzzard", 0x2F03547B));
CModelIndex MI_HELI_BUZZARD2(strStreamingObjectName("buzzard2", 0x2C75F0DD));

CModelIndex MI_TRAILER_TANKER(strStreamingObjectName("tanker", 0xD46F4737));
CModelIndex MI_TRAILER_TANKER2(strStreamingObjectName("tanker2", 0x74998082));

CModelIndex MI_TRAILER_TRAILERLARGE(strStreamingObjectName("trailerlarge", 0x5993F939));
CModelIndex MI_TRAILER_TRAILERSMALL2(strStreamingObjectName("trailersmall2", 0x8FD54EBB));

CModelIndex MI_TRAIN_METRO(strStreamingObjectName("metrotrain", 0x33c9e158));
CModelIndex MI_MONSTER_TRUCK(strStreamingObjectName("Monster", 0xCD93A7DB));
CModelIndex MI_CAR_TACO(strStreamingObjectName("TACO", 0x744CA80D));
CModelIndex MI_CAR_BULLDOZER(strStreamingObjectName("bulldozer", 0x7074F39D));
CModelIndex MI_CAR_BANSHEE2(strStreamingObjectName("banshee2", 0x25C5AF13));
CModelIndex MI_CAR_SLAMVAN3(strStreamingObjectName("slamvan3", 0x42BC5E19));

CModelIndex MI_CAR_PACKER(strStreamingObjectName("packer", 0x21EEE87D));
CModelIndex MI_CAR_PHANTOM(strStreamingObjectName("phantom", 0x809AA4CB));
CModelIndex MI_CAR_BRICKADE(strStreamingObjectName("brickade", 0xEDC6F847));

CModelIndex MI_CAR_CHERNOBOG(strStreamingObjectName("chernobog", 0xD6BC7523));

CModelIndex MI_CAR_DUMP_TRUCK(strStreamingObjectName("dump", 0x810369E2));
CModelIndex MI_CAR_TROPHY_TRUCK(strStreamingObjectName("trophytruck", 0x612F4B6));
CModelIndex MI_CAR_TROPHY_TRUCK2(strStreamingObjectName("trophytruck2", 0xD876DBE2));
CModelIndex MI_CAR_TROPOS(strStreamingObjectName("tropos", 0x707E63A4));
CModelIndex MI_CAR_OMNIS(strStreamingObjectName("omnis", 0xD1AD4937));

CModelIndex MI_CAR_RUINER2(strStreamingObjectName("ruiner2", 0x381E10BD));
CModelIndex MI_VAN_BOXVILLE5(strStreamingObjectName("boxville5", 0x28AD20E1));

CModelIndex MI_TRIKE_CHIMERA(strStreamingObjectName("chimera", 0x675ED7));
CModelIndex MI_TRIKE_RROCKET(strStreamingObjectName("rrocket", 0x36A167E0));
CModelIndex MI_QUADBIKE_BLAZER4(strStreamingObjectName("blazer4", 0xE5BA6858));
CModelIndex MI_CAR_RAPTOR(strStreamingObjectName("raptor", 0xD7C56D39));
CModelIndex MI_BIKE_FAGGIO3(strStreamingObjectName("faggio3", 0xB328B188));

CModelIndex MI_CAR_TECHNICAL2(strStreamingObjectName("technical2", 0x4662BCBB ));
CModelIndex MI_CAR_TECHNICAL3(strStreamingObjectName("technical3", 0x50D4D19F ));

CModelIndex	MI_CAR_DUNE3(strStreamingObjectName("dune3",0x711D4738));
CModelIndex	MI_CAR_DUNE4(strStreamingObjectName("dune4",0xCEB28249));
CModelIndex	MI_CAR_DUNE5(strStreamingObjectName("dune5",0xED62BFA9));
CModelIndex	MI_CAR_PHANTOM2(strStreamingObjectName("phantom2",0x9DAE1398));
CModelIndex	MI_CAR_WASTELANDER(strStreamingObjectName("wastelander",0x8e08ec82));
CModelIndex	MI_CAR_WASTELANDER2(strStreamingObjectName("wastelander2",0xECC7F980));
CModelIndex MI_CAR_NERO(strStreamingObjectName("nero",0x3DA47243));

CModelIndex MI_CAR_HALFTRACK(strStreamingObjectName("halftrack",0xFE141DA6));
CModelIndex MI_CAR_APC(strStreamingObjectName("apc",0x2189D250));
CModelIndex MI_CAR_XA21(strStreamingObjectName("xa21",0x36B4A8A9));
CModelIndex MI_CAR_TORERO(strStreamingObjectName("torero",0x59A9E570));
CModelIndex MI_CAR_TORERO2(strStreamingObjectName("torero2",0xF62446BA));
CModelIndex MI_CAR_INSURGENT2(strStreamingObjectName("insurgent2",0x7B7E56F0));
CModelIndex MI_CAR_INSURGENT3(strStreamingObjectName("insurgent3",0x8d4b7a8a));
CModelIndex MI_CAR_VAGNER(strStreamingObjectName("vagner",0x7397224C));

CModelIndex MI_CAR_PROTO(strStreamingObjectName("prototipo",0x7E8F677F));
CModelIndex MI_CAR_VISIONE(strStreamingObjectName("visione",0xC4810400));

CModelIndex MI_CAR_VIGILANTE(strStreamingObjectName("vigilante",0xB5EF4C33));

CModelIndex MI_CAR_GP1(strStreamingObjectName("gp1",0x4992196C));
CModelIndex MI_CAR_INFERNUS2(strStreamingObjectName("infernus2",0xAC33179C));
CModelIndex MI_CAR_RUSTON(strStreamingObjectName("ruston",0x2AE524A8));

CModelIndex MI_CAR_TAMPA3(strStreamingObjectName("tampa3",0xB7D9F7F1));
CModelIndex MI_CAR_NIGHTSHARK(strStreamingObjectName("nightshark",0x19DD9ED1));
CModelIndex	MI_CAR_PHANTOM3(strStreamingObjectName("phantom3",0xA90ED5C));
CModelIndex	MI_CAR_HAULER2(strStreamingObjectName("hauler2",0x171C92C4));

// DLC - mpChristmas2017
CModelIndex	MI_CAR_DELUXO(strStreamingObjectName("deluxo",0x586765FB));
CModelIndex	MI_CAR_STROMBERG(strStreamingObjectName("stromberg",0x34DBA661));
CModelIndex	MI_HELI_AKULA(strStreamingObjectName("akula",0x46699F47));
CModelIndex	MI_CAR_SENTINEL3(strStreamingObjectName("sentinel3",0x41D149AA));
CModelIndex	MI_CAR_BARRAGE(strStreamingObjectName("barrage",0xF34DFB25));
CModelIndex	MI_PLANE_VOLATOL(strStreamingObjectName("volatol",0x1AAD0DED));
CModelIndex MI_CAR_COMET4(strStreamingObjectName("comet4",0x5D1903F9));
CModelIndex MI_CAR_RAIDEN(strStreamingObjectName("raiden",0xA4D99B7D));
CModelIndex MI_CAR_NEON(strStreamingObjectName("neon",0x91CA96EE));
CModelIndex MI_CAR_Z190(strStreamingObjectName("z190",0x3201DD49));
CModelIndex MI_CAR_VISERIS(strStreamingObjectName("viseris", 0xE8A8BA94));

// DLC - mpAssault
CModelIndex MI_CAR_TEZERACT( strStreamingObjectName( "tezeract", 0x3D7C6410 ) );
CModelIndex MI_HELI_SEASPARROW( strStreamingObjectName( "seasparrow", 0xD4AE63D9 ) );

CModelIndex MI_ROLLER_COASTER_CAR_1(strStreamingObjectName("ind_prop_dlc_roller_car", 0x5C05F6C1));
CModelIndex MI_ROLLER_COASTER_CAR_2(strStreamingObjectName("ind_prop_dlc_roller_car_02", 0x1F584B2E));

CModelIndex MI_CAR_CARACARA(strStreamingObjectName("caracara", 0x4ABEBF23));

// DLC - mpBattle
CModelIndex MI_CAR_SWINGER( strStreamingObjectName( "swinger", 0x1DD4C0FF ) );
CModelIndex MI_CAR_MULE4( strStreamingObjectName( "mule4", 0x73F4110E ) );
CModelIndex MI_CAR_SPEEDO4(strStreamingObjectName("speedo4", 0xD17099D));
CModelIndex MI_CAR_POUNDER2(strStreamingObjectName("pounder2", 0x6290F15B));
CModelIndex MI_CAR_PATRIOT2(strStreamingObjectName("patriot2", 0xE6E967F8));
CModelIndex MI_BATTLE_DRONE_QUAD(strStreamingObjectName("ba_prop_battle_drone_quad",0x8e7b47a7));
CModelIndex MI_BIKE_OPPRESSOR2( strStreamingObjectName( "oppressor2", 0x7B54A9D3 ) );
CModelIndex MI_CAR_PBUS2( strStreamingObjectName( "pbus2", 0x149BD32A ) );
CModelIndex MI_PLANE_STRIKEFORCE( strStreamingObjectName( "strikeforce", 0x64DE07A1 ) );
CModelIndex MI_CAR_MENACER( strStreamingObjectName( "menacer", 0x79DD18AE ) );
CModelIndex MI_CAR_TERBYTE( strStreamingObjectName( "terbyte", 0x897AFC65 ) );
CModelIndex MI_CAR_STAFFORD( strStreamingObjectName( "stafford", 0x1324E960 ) );
CModelIndex MI_CAR_SCRAMJET( strStreamingObjectName( "scramjet", 0xD9F0503D ) );

// DLC - mpChristmas2018
CModelIndex MI_CAR_SCARAB( strStreamingObjectName( "scarab", 0xBBA2A2F7 ) );
CModelIndex MI_CAR_SCARAB2(strStreamingObjectName("scarab2", 0x5BEB3CE0));
CModelIndex MI_CAR_SCARAB3(strStreamingObjectName("scarab3", 0xDD71BFEB));
CModelIndex MI_CAR_RCBANDITO( strStreamingObjectName( "rcbandito", 0xEEF345EC ) );
CModelIndex MI_CAR_BRUISER( strStreamingObjectName( "bruiser", 0x27D79225 ) );
CModelIndex MI_TRUCK_CERBERUS(strStreamingObjectName("cerberus", 0xD039510B));
CModelIndex MI_TRUCK_CERBERUS2(strStreamingObjectName("cerberus2", 0x287FA449));
CModelIndex MI_TRUCK_CERBERUS3(strStreamingObjectName("cerberus3", 0x71D3B6F0));
CModelIndex MI_BIKE_DEATHBIKE( strStreamingObjectName( "deathbike", 0xFE5F0722 ) );
CModelIndex MI_BIKE_DEATHBIKE2( strStreamingObjectName( "deathbike2", 0x93F09558 ) );

// DLC - mpVinewood (casino)
CModelIndex MI_CAR_DYNASTY(strStreamingObjectName("dynasty", 0x127E90D5));
CModelIndex MI_CAR_EMERUS(strStreamingObjectName("emerus", 0x4EE74355));
CModelIndex MI_CAR_HELLION(strStreamingObjectName("hellion", 0xEA6A047F));
CModelIndex MI_CAR_LOCUST(strStreamingObjectName("locust", 0xC7E55211));
CModelIndex MI_CAR_NEO(strStreamingObjectName("neo", 0x9F6ED5A2));
CModelIndex MI_CAR_NOVAK(strStreamingObjectName("novak", 0x92F5024E));
CModelIndex MI_CAR_PEYOTE2(strStreamingObjectName("peyote2", 0x9472CD24));
CModelIndex MI_CAR_S80(strStreamingObjectName("s80", 0xECA6B6A3));
CModelIndex MI_CAR_ZION3(strStreamingObjectName("zion3", 0x6F039A67));
CModelIndex MI_CAR_ZORRUSSO(strStreamingObjectName("zorrusso", 0xD757D97D));

// DLC - mpHeist3
CModelIndex MI_CAR_FURIA( strStreamingObjectName( "furia", 0x3944D5A0 ) );
CModelIndex MI_CAR_STRYDER( strStreamingObjectName( "stryder", 0x11F58A5A ) );
CModelIndex MI_CAR_ZHABA(strStreamingObjectName("zhaba", 0x4C8DBA51));
CModelIndex MI_CAR_MINITANK( strStreamingObjectName( "minitank", 0xB53C6C52 ) );
CModelIndex MI_CAR_FORMULA(strStreamingObjectName("formula", 0x1446590A));
CModelIndex MI_CAR_FORMULA2(strStreamingObjectName("formula2", 0x8B213907));

// DLC - CnC
CModelIndex MI_CAR_OPENWHEEL1(strStreamingObjectName("Openwheel1", 0x58F77553));
CModelIndex MI_CAR_OPENWHEEL2(strStreamingObjectName("Openwheel2", 0x4669D038));

// DLC - Heist4
CModelIndex MI_CAR_BRIOSO2( strStreamingObjectName( "Brioso2", 0x55365079 ) );
CModelIndex MI_CAR_TOREADOR( strStreamingObjectName( "Toreador", 0x56C8A5EF ) );
CModelIndex MI_SUB_KOSATKA( strStreamingObjectName( "Kosatka", 0x4FAF0D70 ) );
CModelIndex MI_CAR_ITALIRSX( strStreamingObjectName( "Italirsx", 0xBB78956A ) );
CModelIndex MI_SUB_AVISA( strStreamingObjectName( "Avisa", 0x9A474B5E ) );
CModelIndex MI_HELI_ANNIHILATOR2( strStreamingObjectName( "Annihilator2", 0x11962E49 ) );
CModelIndex MI_BOAT_PATROLBOAT( strStreamingObjectName( "Patrolboat", 0xEF813606 ) );
CModelIndex MI_HELI_SEASPARROW2( strStreamingObjectName( "seasparrow2", 0x494752F7 ) );
CModelIndex MI_HELI_SEASPARROW3( strStreamingObjectName( "seasparrow3", 0x5F017E6B ) );
CModelIndex MI_CAR_VETO(strStreamingObjectName( "veto", 0xCCE5C8FA) );
CModelIndex MI_CAR_VETO2(strStreamingObjectName( "veto2", 0xA703E4A9) );
CModelIndex MI_PLANE_ALKONOST(strStreamingObjectName( "Alkonost", 0xEA313705) );
CModelIndex MI_BOAT_DINGHY5(strStreamingObjectName( "Dinghy5", 0xC58DA34A) );
CModelIndex MI_CAR_SLAMTRUCK(strStreamingObjectName( "Slamtruck", 0xC1A8A914) );

//DLC - Tuner
CModelIndex MI_CAR_CALICO(strStreamingObjectName("Calico", 0xB8D657AD));
CModelIndex MI_CAR_COMET6( strStreamingObjectName( "Comet6", 0x991EFC04 ) );
CModelIndex MI_CAR_CYPHER(strStreamingObjectName("Cypher", 0x68A5D1EF));
CModelIndex MI_CAR_DOMINATOR7(strStreamingObjectName("Dominator7", 0x196F9418));
CModelIndex MI_CAR_DOMINATOR8(strStreamingObjectName("Dominator8", 0x2BE8B90A));
CModelIndex MI_CAR_EUROS(strStreamingObjectName("Euros", 0x7980BDD5));
CModelIndex MI_CAR_FREIGHTCAR2(strStreamingObjectName("FreightCar2", 0xBDEC3D99));
CModelIndex MI_CAR_FUTO2( strStreamingObjectName( "Futo2", 0xA6297CC8) );
CModelIndex MI_CAR_GROWLER(strStreamingObjectName("Growler", 0x4DC079D7));
CModelIndex MI_CAR_JESTER4(strStreamingObjectName("Jester4", 0xA1B3A871));
CModelIndex MI_CAR_PREVION(strStreamingObjectName("Previon", 0x546DA331));
CModelIndex MI_CAR_REMUS(strStreamingObjectName("Remus", 0x5216AD5E));
CModelIndex MI_CAR_RT3000(strStreamingObjectName("RT3000", 0xE505CF99));
CModelIndex MI_CAR_SULTAN3(strStreamingObjectName("Sultan3", 0xEEA75E63));
CModelIndex MI_CAR_TAILGATER2(strStreamingObjectName("Tailgater2", 0xB5D306A4));
CModelIndex MI_CAR_VECTRE(strStreamingObjectName("Vectre", 0xA42FC3A5));
CModelIndex MI_CAR_WARRENER2(strStreamingObjectName("Warrener2", 0x2290C50A));
CModelIndex MI_CAR_ZR350(strStreamingObjectName("ZR350", 0x91373058));

//DLC - Fixer
CModelIndex MI_CAR_ASTRON(strStreamingObjectName("astron", 0x258C9364));
CModelIndex MI_CAR_BALLER7(strStreamingObjectName("baller7", 0x1573422D));
CModelIndex MI_CAR_BUFFALO4(strStreamingObjectName("buffalo4", 0xDB0C9B04));
CModelIndex MI_CAR_CHAMPION(strStreamingObjectName("champion", 0xC972A155));
CModelIndex MI_CAR_CINQUEMILA(strStreamingObjectName("cinquemila", 0xA4F52C13));
CModelIndex MI_CAR_COMET7(strStreamingObjectName("comet7", 0x440851D8));
CModelIndex MI_CAR_DEITY(strStreamingObjectName("deity", 0x5B531351));
CModelIndex MI_CAR_GRANGER2(strStreamingObjectName("granger2", 0xF06C29C7));
CModelIndex MI_CAR_IGNUS(strStreamingObjectName("ignus", 0xA9EC907B));
CModelIndex MI_CAR_IWAGEN(strStreamingObjectName("iwagen", 0x27816B7E));
CModelIndex MI_CAR_JUBILEE(strStreamingObjectName("jubilee", 0x1B8165D3));
CModelIndex MI_CAR_PATRIOT3(strStreamingObjectName("patriot3", 0xD80F4A44));
CModelIndex MI_CAR_REEVER(strStreamingObjectName("reever", 0x76D7C404));
CModelIndex MI_CAR_SHINOBI(strStreamingObjectName("shinobi", 0x50A6FB9C));
CModelIndex MI_CAR_ZENO(strStreamingObjectName("zeno", 0x2714AA93));
CModelIndex MI_CAR_TENF(strStreamingObjectName("tenf", 0xCAB6E261));
CModelIndex MI_CAR_TENF2(strStreamingObjectName("tenf2", 0x10635A0E));
CModelIndex MI_CAR_OMNISEGT(strStreamingObjectName("omnisegt", 0xE1E2E6D7));

//DLC - Gen9
CModelIndex MI_CAR_IGNUS2(strStreamingObjectName("Ignus2", 0x39085F47));

//DLC - Gen9 HSW Upgrades
CModelIndex MI_CAR_DEVESTE(strStreamingObjectName("deveste", 0x5EE005DA));
CModelIndex MI_CAR_BRIOSO(strStreamingObjectName("brioso", 0x5C55CB39));
CModelIndex MI_CAR_TURISMO2(strStreamingObjectName("turismo2", 0xC575DF11));
CModelIndex MI_BIKE_HAKUCHOU2(strStreamingObjectName("hakuchou2", 0xF0C2A91F));

// Peds:
// ped superlod
CModelIndex MI_PED_SLOD_HUMAN(strStreamingObjectName("slod_human",0x3F039CBA));
CModelIndex MI_PED_SLOD_SMALL_QUADPED(strStreamingObjectName("slod_small_quadped",0x2D7030F3));
CModelIndex MI_PED_SLOD_LARGE_QUADPED(strStreamingObjectName("slod_large_quadped",0x856CFB02));

CModelIndex MI_PED_COP(strStreamingObjectName("S_M_Y_Cop_01",0x5E3DA4A4));
CModelIndex MI_PED_ORLEANS(strStreamingObjectName("IG_Orleans",0x61d4c771));

// Animal models
CModelIndex MI_PED_ANIMAL_COW(strStreamingObjectName("A_C_Cow", 0xFCFA9E1E));
CModelIndex MI_PED_ANIMAL_DEER(strStreamingObjectName("A_C_Deer", 0xD86B5A95));

// Player models
CModelIndex MI_PLAYERPED_PLAYER_ZERO(strStreamingObjectName("Player_Zero",0xD7114C9));
CModelIndex MI_PLAYERPED_PLAYER_ONE(strStreamingObjectName("Player_One",0x9B22DBAF));
CModelIndex MI_PLAYERPED_PLAYER_TWO(strStreamingObjectName("Player_Two",0x9B810FA2));

// Parachute:
CModelIndex MI_PED_PARACHUTE(strStreamingObjectName("P_Parachute1_SP_S",0xd06c8df));

// Problematic objects which an AI ped might become stuck inside
CModelIndex MI_PROP_BIN_14A(strStreamingObjectName("prop_bin_14a",0x683475ee));

// Problematic fragable objects which player has difficultly with cover
CModelIndex MI_PROP_BOX_WOOD04A(strStreamingObjectName("prop_box_wood04a",0xb131133a));

// For Sale signs used by the property exterior script in MP
CModelIndex MI_PROP_FORSALE_DYN_01(strStreamingObjectName("PROP_FORSALE_DYN_01",0x4728C7F2));
CModelIndex MI_PROP_FORSALE_DYN_02(strStreamingObjectName("PROP_FORSALE_DYN_02",0x116B5C78));

CModelIndex MI_PROP_STUNT_TUBE_01(strStreamingObjectName("stt_prop_stunt_tube_fn_01",0x64a8a51f));
CModelIndex MI_PROP_STUNT_TUBE_02(strStreamingObjectName("stt_prop_stunt_tube_fn_02",0xb66f48ab));
CModelIndex MI_PROP_STUNT_TUBE_03(strStreamingObjectName("stt_prop_stunt_tube_fn_03",0xc885ecd8));
CModelIndex MI_PROP_STUNT_TUBE_04(strStreamingObjectName("stt_prop_stunt_tube_fn_04",0x9ac0914e));
CModelIndex MI_PROP_STUNT_TUBE_05(strStreamingObjectName("stt_prop_stunt_tube_fn_05",0x893c6e3a));

CModelIndex MI_PROP_FOOTBALL(strStreamingObjectName("stt_prop_stunt_soccer_sball",0x286F5F0));
CModelIndex MI_PROP_STUNT_FOOTBALL(strStreamingObjectName("stt_prop_stunt_soccer_lball",0xDB2C3E38));
CModelIndex MI_PROP_STUNT_FOOTBALL2(strStreamingObjectName("stt_prop_stunt_soccer_ball",0xC00C3530));
