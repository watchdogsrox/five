// 
// events_durango.winrt.cpp 
// 
// Copyright (C) 1999-2013 Rockstar Games.  All Rights Reserved.
// 

#if RSG_DURANGO

#include "diag/channel.h"
#include "fwnet/netchannel.h"

#include "diag/seh.h"
#include "events_durango.h"
#include "Network/xlast/Events.man.h"
#include "Network/xlast/Events_JA.man.h"
#include "Network/xlast/Fuzzy.schema.h"
#include "Network/Live/livemanager.h"
#include "Network/Live/NetworkRichPresence.h"
#include "rline/durango/rlxbl_interface.h"
#include "rline/durango/private/rlxblcommon.h"
#include "Peds/Ped.h"
#include "rline/durango/private/rlxblinternal.h"
#include "rline/durango/rlxblachievements_interface.h"
#include "frontend/FrontendStatsMgr.h"
#include "system/appcontent.h"

#include <xdk.h>

using namespace Platform;
using namespace Windows::Foundation;

using namespace rage;

NETWORK_OPTIMISATIONS()

RAGE_DEFINE_SUBCHANNEL(net, durangoevents)
#undef __net_channel
#define __net_channel net_durangoevents

CompileTimeAssert(events_durango::XB1_ACHIEVEMENT_EVENTS == (player_schema::Achievements::COUNT+1)); // +1 for platinum index

Functor3Ret<ULONG, PCWSTR,LPCGUID,int> events_durango::m_fAchievementEvents[XB1_ACHIEVEMENT_EVENTS] =
{
	MakeFunctorRet(&events_durango::NoOpAch),			// PLATINUM = 0, 
	MakeFunctorRet(&EventWriteACH01_Event),				// ACH00 = 1,		Welcome to Los Santos
	MakeFunctorRet(&EventWriteACH02_Event),				// ACH01 = 2,		A Friendship Resurrected
	MakeFunctorRet(&EventWriteACH03_Event),				// ACH02 = 3,		A Fair Day's Pay
	MakeFunctorRet(&EventWriteACH04_Event),				// ACH03 = 4,		The Moment of Truth
	MakeFunctorRet(&EventWriteACH05_Event),				// ACH04 = 5,		To Live or Die in Los Santos
	MakeFunctorRet(&EventWriteACH06_Event),				// ACH05 = 6,		Diamond Hard
	MakeFunctorRet(&EventWriteACH07_Event),				// ACH06 = 7,		Subversive
	MakeFunctorRet(&EventWriteACH08_Event),				// ACH42 = 8,		Blitzed
	MakeFunctorRet(&EventWriteACH09_Event),				// ACH07 = 9,		Small Town, Big Job
	MakeFunctorRet(&EventWriteACH10_Event),				// ACH08 = 10,		The Government Gimps
	MakeFunctorRet(&EventWriteACH11_Event),				// ACH09 = 11,		The Big One!
	MakeFunctorRet(&EventWriteACH12_Event),				// ACH10 = 12,		Solid Gold, Baby!
	MakeFunctorRet(&EventWriteACH13_Event),				// ACH11 = 13,		Career Criminal
	MakeFunctorRet(&EventWriteACH14_Event),				// ACH50 = 14,		San Andreas Sightseer
	MakeFunctorRet(&EventWriteACH15_Event),				// ACH12 = 15,		All's Fare in Love and War
	MakeFunctorRet(&EventWriteACH16_Event),				// ACH13 = 16,		TP Industries Arms Race
	MakeFunctorRet(&EventWriteACH17_Event),				// ACH14 = 17,		Multi-Disciplined
	MakeFunctorRet(&EventWriteACH18_Event),				// ACH15 = 18,		From Beyond the Stars
	MakeFunctorRet(&EventWriteACH19_Event),				// ACH16 = 19,		A Mystery, Solved - Solve the mystery of Leonora Johnson. (Collect 50 Letter Scraps + 1 for turnin)
	MakeFunctorRet(&EventWriteACH20_Event),				// ACH17 = 20,		Waste Management 
	MakeFunctorRet(&EventWriteACH21_Event),				// ACH18 = 21,		Red Mist
	MakeFunctorRet(&EventWriteACH22_Event),				// ACH19 = 22,		Show Off
	MakeFunctorRet(&EventWriteACH23_Event),				// ACH20 = 23,		Kifflom!
	MakeFunctorRet(&EventWriteACH24_Event),				// ACH21 = 24,		Three Man Army
	MakeFunctorRet(&EventWriteACH25_Event),				// ACH22 = 25,		Out of Your Depth
	MakeFunctorRet(&EventWriteACH26_Event),				// ACH23 = 26,		Altruist Acolyte
	MakeFunctorRet(&EventWriteACH27_Event),				// ACH24 = 27,		A Lot of Cheddar - Spend a total of $200 million across all three characters.
	MakeFunctorRet(&EventWriteACH28_Event),				// ACH25 = 28,		Trading Pure Alpha
	MakeFunctorRet(&EventWriteACH29_Event),				// ACH26 = 29,		Pimp My Sidearm
	MakeFunctorRet(&EventWriteACH30_Event),				// ACH27 = 30,		Wanted: Alive Or Alive
	MakeFunctorRet(&EventWriteACH31_Event),				// ACH28 = 31,		Los Santos Customs
	MakeFunctorRet(&EventWriteACH32_Event),				// ACH29 = 32,		Close Shave
	MakeFunctorRet(&EventWriteACH33_Event),				// ACH30 = 33,		Off the Plane
	MakeFunctorRet(&EventWriteHighestRankMP),			// ACH31 = 34,		Three-Bit Gangster
	MakeFunctorRet(&EventWriteHighestRankMP),			// ACH32 = 35,		Making Moves
	MakeFunctorRet(&EventWriteHighestRankMP),			// ACH33 = 36,		Above the Law
	MakeFunctorRet(&EventWriteACH37_Event),				// ACH34 = 37,		Numero Uno
	MakeFunctorRet(&EventWriteTotalCustomRacesWon),		// ACH35 = 38,		The Midnight Club
	MakeFunctorRet(&EventWriteHordeWavesSurvived),		// ACH36 = 39,		Unnatural Selection
	MakeFunctorRet(&EventWriteACH40_Event),				// ACH38 = 40,		Backseat Driver
	MakeFunctorRet(&EventWriteACH41_Event),				// ACH39 = 41,		Run Like The Wind
	MakeFunctorRet(&EventWriteACH42_Event),				// ACH40 = 42,		Clean Sweep
	MakeFunctorRet(&EventWritePlatinumAwards),			// ACH41 = 43,		Decorated
	MakeFunctorRet(&EventWriteHeldUpShops),				// ACH43 = 44,		Stick Up Kid
	MakeFunctorRet(&EventWriteACH45_Event),				// ACH45 = 45,		Enjoy Your Stay
	MakeFunctorRet(&EventWriteACH46_Event),				// ACH46 = 46,		Crew Cut
	MakeFunctorRet(&EventWriteACH47_Event),				// ACH47 = 47,		Full Refund
	MakeFunctorRet(&EventWriteACH48_Event),				// ACH48 = 48,		Dialling Digits
	MakeFunctorRet(&EventWriteACH49_Event),				// ACH49 = 49,		American Dream
	MakeFunctorRet(&EventWriteACH51_Event),				// ACH51 = 50,		A New Perspective
	// HEISTS
	MakeFunctorRet(&EventWriteACHH1_Event),			// ACHH1 = 51,		Be Prepared
	MakeFunctorRet(&EventWriteACHH2_Event),			// ACHH2 = 52,		In the Name of Science
	MakeFunctorRet(&EventWriteACHH3_Event),			// ACHH3 = 53,		Dead Presidents 
	MakeFunctorRet(&EventWriteACHH4_Event),			// ACHH4 = 54,		Parole Day
	MakeFunctorRet(&EventWriteACHH5_Event),			// ACHH5 = 55,		Shot Caller
	MakeFunctorRet(&EventWriteACHH6_Event),			// ACHH6 = 56,		Four Way
	MakeFunctorRet(&EventWriteACHH7_Event),			// ACHH7 = 57,		Live a Little
	MakeFunctorRet(&EventWriteACHH8_Event),			// ACHH8 = 58,		Can't Touch This
							// ach h9 was removed
	MakeFunctorRet(&EventWriteACHH10_Event),		// ACHH10 = 59,		Mastermind

	MakeFunctorRet(&EventWriteACHH11_Event),		// ACHH11 = 60,		VINEWOOD VISIONARY 
	MakeFunctorRet(&EventWriteACHR2_Event),			// ACHR2  = 61,		MAJESTIC
	MakeFunctorRet(&EventWriteACHR3_Event),			// ACHR3  = 62,		HUMANS OF LOS SANTOS
	MakeFunctorRet(&EventWriteACHR4_Event),			// ACHR4  = 63,		FIRST TIME DIRECTOR
	MakeFunctorRet(&EventWriteACHR5_Event),			// ACHR5  = 64,		ANIMAL LOVER
	MakeFunctorRet(&EventWriteACHR6_Event),			// ACHR6  = 65,		ENSEMBLE PIECE
	MakeFunctorRet(&EventWriteACHR7_Event),			// ACHR7  = 66,		CULT MOVIE
	MakeFunctorRet(&EventWriteACHR8_Event),			// ACHR8  = 67,		LOCATION SCOUT
	MakeFunctorRet(&EventWriteACHR9_Event),			// ACHR9  = 68,		METHOD ACTOR
	MakeFunctorRet(&EventWriteACHR10_Event),		// ACHR10 = 69,		CRYTPOZOOLOGIST [SECRET]

	//Heists 2 Update
	MakeFunctorRet(&EventWriteACHGO1_Event),		// ACHGO1 = 70, Getting Started
	MakeFunctorRet(&EventWriteACHGO2_Event),		// ACHGO2 = 71, The Data Breaches
	MakeFunctorRet(&EventWriteACHGO3_Event),		// ACHGO3 = 72, The Bogdan Problem
	MakeFunctorRet(&EventWriteACHGO4_Event),		// ACHGO4 = 73, The Doomsday Scenario
	MakeFunctorRet(&EventWriteACHGO5_Event),		// ACHGO5 = 74, A World Worth Saving
	MakeFunctorRet(&EventWriteACHGO6_Event),		// ACHGO6 = 75, Orbital Obliteration
	MakeFunctorRet(&EventWriteACHGO7_Event),		// ACHGO7 = 76, Elitist
	MakeFunctorRet(&EventWriteACHGO8_Event),		// ACHGO8 = 77, Masterminds
};

Functor3Ret<ULONG, PCWSTR,LPCGUID,int> events_durango::m_fAchievementEventsJA[XB1_ACHIEVEMENT_EVENTS] =
{
	MakeFunctorRet(&events_durango::NoOpAch),			    // PLATINUM = 0, 
	MakeFunctorRet(&EventWriteJA_ACH01_Event),				// ACH00 = 1,		Welcome to Los Santos
	MakeFunctorRet(&EventWriteJA_ACH02_Event),				// ACH01 = 2,		A Friendship Resurrected
	MakeFunctorRet(&EventWriteJA_ACH03_Event),				// ACH02 = 3,		A Fair Day's Pay
	MakeFunctorRet(&EventWriteJA_ACH04_Event),				// ACH03 = 4,		The Moment of Truth
	MakeFunctorRet(&EventWriteJA_ACH05_Event),				// ACH04 = 5,		To Live or Die in Los Santos
	MakeFunctorRet(&EventWriteJA_ACH06_Event),				// ACH05 = 6,		Diamond Hard
	MakeFunctorRet(&EventWriteJA_ACH07_Event),				// ACH06 = 7,		Subversive
	MakeFunctorRet(&EventWriteJA_ACH08_Event),				// ACH42 = 8,		Blitzed
	MakeFunctorRet(&EventWriteJA_ACH09_Event),				// ACH07 = 9,		Small Town, Big Job
	MakeFunctorRet(&EventWriteJA_ACH10_Event),				// ACH08 = 10,		The Government Gimps
	MakeFunctorRet(&EventWriteJA_ACH11_Event),				// ACH09 = 11,		The Big One!
	MakeFunctorRet(&EventWriteJA_ACH12_Event),				// ACH10 = 12,		Solid Gold, Baby!
	MakeFunctorRet(&EventWriteJA_ACH13_Event),				// ACH11 = 13,		Career Criminal
	MakeFunctorRet(&EventWriteJA_ACH14_Event),				// ACH50 = 14,		San Andreas Sightseer
	MakeFunctorRet(&EventWriteJA_ACH15_Event),				// ACH12 = 15,		All's Fare in Love and War
	MakeFunctorRet(&EventWriteJA_ACH16_Event),				// ACH13 = 16,		TP Industries Arms Race
	MakeFunctorRet(&EventWriteJA_ACH17_Event),				// ACH14 = 17,		Multi-Disciplined
	MakeFunctorRet(&EventWriteJA_ACH18_Event),				// ACH15 = 18,		From Beyond the Stars
	MakeFunctorRet(&EventWriteJA_ACH19_Event),				// ACH16 = 19,		A Mystery, Solved - Solve the mystery of Leonora Johnson. (Collect 50 Letter Scraps + 1 for turnin)
	MakeFunctorRet(&EventWriteJA_ACH20_Event),				// ACH17 = 20,		Waste Management 
	MakeFunctorRet(&EventWriteJA_ACH21_Event),				// ACH18 = 21,		Red Mist
	MakeFunctorRet(&EventWriteJA_ACH22_Event),				// ACH19 = 22,		Show Off
	MakeFunctorRet(&EventWriteJA_ACH23_Event),				// ACH20 = 23,		Kifflom!
	MakeFunctorRet(&EventWriteJA_ACH24_Event),				// ACH21 = 24,		Three Man Army
	MakeFunctorRet(&EventWriteJA_ACH25_Event),				// ACH22 = 25,		Out of Your Depth
	MakeFunctorRet(&EventWriteJA_ACH26_Event),				// ACH23 = 26,		Altruist Acolyte
	MakeFunctorRet(&EventWriteJA_ACH27_Event),				// ACH24 = 27,		A Lot of Cheddar - Spend a total of $200 million across all three characters.
	MakeFunctorRet(&EventWriteJA_ACH28_Event),				// ACH25 = 28,		Trading Pure Alpha
	MakeFunctorRet(&EventWriteJA_ACH29_Event),				// ACH26 = 29,		Pimp My Sidearm
	MakeFunctorRet(&EventWriteJA_ACH30_Event),				// ACH27 = 30,		Wanted: Alive Or Alive
	MakeFunctorRet(&EventWriteJA_ACH31_Event),				// ACH28 = 31,		Los Santos Customs
	MakeFunctorRet(&EventWriteJA_ACH32_Event),				// ACH29 = 32,		Close Shave
	MakeFunctorRet(&EventWriteJA_ACH33_Event),				// ACH30 = 33,		Off the Plane
	MakeFunctorRet(&EventWriteJA_HighestRankMP),			// ACH31 = 34,		Three-Bit Gangster
	MakeFunctorRet(&EventWriteJA_HighestRankMP),			// ACH32 = 35,		Making Moves
	MakeFunctorRet(&EventWriteJA_HighestRankMP),			// ACH33 = 36,		Above the Law
	MakeFunctorRet(&EventWriteJA_ACH37_Event),				// ACH34 = 37,		Numero Uno
	MakeFunctorRet(&EventWriteJA_TotalCustomRacesWon),		// ACH35 = 38,		The Midnight Club
	MakeFunctorRet(&EventWriteJA_HordeWavesSurvived),		// ACH36 = 39,		Unnatural Selection
	MakeFunctorRet(&EventWriteJA_ACH40_Event),				// ACH38 = 40,		Backseat Driver
	MakeFunctorRet(&EventWriteJA_ACH41_Event),				// ACH39 = 41,		Run Like The Wind
	MakeFunctorRet(&EventWriteJA_ACH42_Event),				// ACH40 = 42,		Clean Sweep
	MakeFunctorRet(&EventWriteJA_PlatinumAwards),			// ACH41 = 43,		Decorated
	MakeFunctorRet(&EventWriteJA_HeldUpShops),				// ACH43 = 44,		Stick Up Kid
	MakeFunctorRet(&EventWriteJA_ACH45_Event),				// ACH45 = 45,		Enjoy Your Stay
	MakeFunctorRet(&EventWriteJA_ACH46_Event),				// ACH46 = 46,		Crew Cut
	MakeFunctorRet(&EventWriteJA_ACH47_Event),				// ACH47 = 47,		Full Refund
	MakeFunctorRet(&EventWriteJA_ACH48_Event),				// ACH48 = 48,		Dialling Digits
	MakeFunctorRet(&EventWriteJA_ACH49_Event),				// ACH49 = 49,		American Dream
	MakeFunctorRet(&EventWriteJA_ACH51_Event),				// ACH51 = 50,		A New Perspective
	// HEISTS
	MakeFunctorRet(&EventWriteJA_ACHH1_Event),			// ACHH1 = 51,		Be Prepared
	MakeFunctorRet(&EventWriteJA_ACHH2_Event),			// ACHH2 = 52,		In the Name of Science
	MakeFunctorRet(&EventWriteJA_ACHH3_Event),			// ACHH3 = 53,		Dead Presidents 
	MakeFunctorRet(&EventWriteJA_ACHH4_Event),			// ACHH4 = 54,		Parole Day
	MakeFunctorRet(&EventWriteJA_ACHH5_Event),			// ACHH5 = 55,		Shot Caller
	MakeFunctorRet(&EventWriteJA_ACHH6_Event),			// ACHH6 = 56,		Four Way
	MakeFunctorRet(&EventWriteJA_ACHH7_Event),			// ACHH7 = 57,		Live a Little
	MakeFunctorRet(&EventWriteJA_ACHH8_Event),			// ACHH8 = 58,		Can't Touch This
							// ach h9 was removed
	MakeFunctorRet(&EventWriteJA_ACHH10_Event),			// ACHH10 = 59		Mastermind

	MakeFunctorRet(&EventWriteJA_ACHH11_Event),			// ACHH11 = 60,		VINEWOOD VISIONARY 	
	MakeFunctorRet(&EventWriteJA_ACHR2_Event),			// ACHR2  = 61,		MAJESTIC	
	MakeFunctorRet(&EventWriteJA_ACHR3_Event),			// ACHR3  = 62,		HUMANS OF LOS SANTOS	
	MakeFunctorRet(&EventWriteJA_ACHR4_Event),			// ACHR4  = 63,		FIRST TIME DIRECTOR	
	MakeFunctorRet(&EventWriteJA_ACHR5_Event),			// ACHR5  = 64,		ANIMAL LOVER	
	MakeFunctorRet(&EventWriteJA_ACHR6_Event),			// ACHR6  = 65,		ENSEMBLE PIECE	
	MakeFunctorRet(&EventWriteJA_ACHR7_Event),			// ACHR7  = 66,		CULT MOVIE	
	MakeFunctorRet(&EventWriteJA_ACHR8_Event),			// ACHR8  = 67,		LOCATION SCOUT	
	MakeFunctorRet(&EventWriteJA_ACHR9_Event),			// ACHR9  = 68,		METHOD ACTOR	
	MakeFunctorRet(&EventWriteJA_ACHR10_Event),			// ACHR10 = 69,		CRYTPOZOOLOGIST [SECRET]	

	//Heists 2 Update
	MakeFunctorRet(&EventWriteJA_ACHGO1_Event),			// ACHGO1 = 70, Getting Started
	MakeFunctorRet(&EventWriteJA_ACHGO2_Event),			// ACHGO2 = 71, The Data Breaches
	MakeFunctorRet(&EventWriteJA_ACHGO3_Event),			// ACHGO3 = 72, The Bogdan Problem
	MakeFunctorRet(&EventWriteJA_ACHGO4_Event),			// ACHGO4 = 73, The Doomsday Scenario
	MakeFunctorRet(&EventWriteJA_ACHGO5_Event),			// ACHGO5 = 74, A World Worth Saving
	MakeFunctorRet(&EventWriteJA_ACHGO6_Event),			// ACHGO6 = 75, Orbital Obliteration
	MakeFunctorRet(&EventWriteJA_ACHGO7_Event),			// ACHGO7 = 76, Elitist
	MakeFunctorRet(&EventWriteJA_ACHGO8_Event),			// ACHGO8 = 77, Masterminds
};

int events_durango::m_iAchievementThresholds[XB1_ACHIEVEMENT_EVENTS] =
{
	1, // PLATINUM = 0, 
	1, // ACH00 = 1,			Welcome to Los Santos
	1, // ACH01 = 2,			A Friendship Resurrected
	1, // ACH02 = 3,			A Fair Day's Pay
	1, // ACH03 = 4,			The Moment of Truth
	1, // ACH04 = 5,			To Live or Die in Los Santos
	1, // ACH05 = 6,			Diamond Hard
	1, // ACH06 = 7,			Subversive
	1, // ACH42 = 8,			Blitzed
	1, // ACH07 = 9,			Small Town, Big Job
	1, // ACH08 = 10,			The Government Gimps
	1, // ACH09 = 11,			The Big One!
	70, // ACH10 = 12,			Solid Gold, Baby!
	100, // ACH11 = 13,			Career Criminal
	1, // ACH50 = 14,			San Andreas Sightseer
	1, // ACH12 = 15,			All's Fare in Love and War
	10, // ACH13 = 16,			TP Industries Arms Race
	6, // ACH14 = 17,			Multi-Disciplined
	51, // ACH15 = 18,			From Beyond the Stars
	51, // ACH16 = 19,			A Mystery, Solved - Solve the mystery of Leonora Johnson. (Collect 50 Letter Scraps + 1 for turnin)
	31, // ACH17 = 20,			Waste Management 
	5, // ACH18 = 21,			Red Mist
	50, // ACH19 = 22,			Show Off
	20, // ACH20 = 23,			Kifflom!
	1, // ACH21 = 24,			Three Man Army
	1, // ACH22 = 25,			Out of Your Depth
	1, // ACH23 = 26,			Altruist Acolyte
	200000000, // ACH24 = 27,	A Lot of Cheddar - Spend a total of $200 million across all three characters.
	1, // ACH25 = 28,			Trading Pure Alpha
	1, // ACH26 = 29,			Pimp My Sidearm
	1, // ACH27 = 30,			Wanted: Alive Or Alive
	1, // ACH28 = 31,			Los Santos Customs
	65, // ACH29 = 32,			Close Shave
	1, // ACH30 = 33,			Off the Plane
	25, // ACH31 = 34,			Three-Bit Gangster
	50, // ACH32 = 35,			Making Moves
	100, // ACH33 = 36,			Above the Law
	1, // ACH34 = 37,			Numero Uno
	5, // ACH35 = 38,			The Midnight Club
	10, // ACH36 = 39,			Unnatural Selection
	1, // ACH38 = 40,			Backseat Driver
	1, // ACH39 = 41,			Run Like The Wind
	1, // ACH40 = 42,			Clean Sweep
	30, // ACH41 = 43,			Decorated
	20, // ACH43 = 44,			Stick Up Kid
	1, // ACH45 = 45,			Enjoy Your Stay
	1, // ACH46 = 46,			Crew Cut
	1, // ACH47 = 47,			Full Refund
	1, // ACH48 = 48,			Dialling Digits
	1, // ACH49 = 49,			American Dream
	1, // ACH51 = 50,			A New Perspective
	// HEISTS
	1, // ACHH1 = 51,		Be Prepared
	1, // ACHH2 = 52,		In the Name of Science
	1, // ACHH3 = 53,		Dead Presidents 
	1, // ACHH4 = 54,		Parole Day
	1, // ACHH5 = 55,		Shot Caller
	1, // ACHH6 = 56,		Four Way
	8000000, // ACHH7 = 57,		Live a Little
	1, // ACHH8 = 58,		Can't Touch This
	// ach h9 was removed
	25, // ACHH10 = 59,		Mastermind
	1, // ACHH11 = 60,      VINEWOOD VISIONARY 
	10, // ACHR2 = 61,       MAJESTIC
	1, // ACHR3 = 62,       HUMANS OF LOS SANTOS
	1, // ACHR4 = 63,       FIRST TIME DIRECTOR
	1, // ACHR5 = 64,       ANIMAL LOVER
	1, // ACHR6 = 65,       ENSEMBLE PIECE
	1, // ACHR7 = 66,       CULT MOVIE
	1, // ACHR8 = 67,       LOCATION SCOUT
	1, // ACHR9 = 68,       METHOD ACTOR
	1, // ACHR10 = 69,      CRYTPOZOOLOGIST [SECRET]
	1, // ACHGO1 = 70,		Getting Started
	1, // ACHGO2 = 71,		The Data Breaches
	1, // ACHGO3 = 72,		The Bogdan Problem
	1, // ACHGO4 = 73,		The Doomsday Scenario
	1, // ACHGO5 = 74,		A World Worth Saving
	1, // ACHGO6 = 75,		Orbital Obliteration
	1, // ACHGO7 = 77,		Elitist
	1, // ACHGO8 = 78,		Masterminds
};

Functor3Ret<ULONG, PCWSTR,LPCGUID,PCWSTR> events_durango::m_fPresenceContextEvents[XB1_PRESENCE_CONTEXT_EVENTS] =
{
	MakeFunctorRet(&events_durango::NoOpPresence),	// PRESENCE_PRES_0
	MakeFunctorRet(&EventWriteCT_SP_MISSIONS),		// PRESENCE_PRES_1
	MakeFunctorRet(&events_durango::NoOpPresence),	// PRESENCE_PRES_2
	MakeFunctorRet(&events_durango::NoOpPresence),	// PRESENCE_PRES_3
	MakeFunctorRet(&events_durango::NoOpPresence),	// PRESENCE_PRES_4
	MakeFunctorRet(&EventWriteCT_DEATHM_LOC),		// PRESENCE_PRES_5
	MakeFunctorRet(&events_durango::NoOpPresence),	// PRESENCE_PRES_6
	MakeFunctorRet(&EventWriteCT_MP_MISSIONS),		// PRESENCE_PRES_7
	MakeFunctorRet(&EventWriteCT_SPMG),				// PRESENCE_PRES_8
	MakeFunctorRet(&EventWriteCT_SPRC),				// PRESENCE_PRES_9
	MakeFunctorRet(&events_durango::NoOpPresence)	// PRESENCE_PRES_10
};

Functor3Ret<ULONG, PCWSTR,LPCGUID,PCWSTR> events_durango::m_fPresenceContextEventsJA[XB1_PRESENCE_CONTEXT_EVENTS] =
{
	MakeFunctorRet(&events_durango::NoOpPresence),	// PRESENCE_PRES_0
	MakeFunctorRet(&EventWriteJA_CT_SP_MISSIONS),		// PRESENCE_PRES_1
	MakeFunctorRet(&events_durango::NoOpPresence),	// PRESENCE_PRES_2
	MakeFunctorRet(&events_durango::NoOpPresence),	// PRESENCE_PRES_3
	MakeFunctorRet(&events_durango::NoOpPresence),	// PRESENCE_PRES_4
	MakeFunctorRet(&EventWriteJA_CT_DEATHM_LOC),		// PRESENCE_PRES_5
	MakeFunctorRet(&events_durango::NoOpPresence),	// PRESENCE_PRES_6
	MakeFunctorRet(&EventWriteJA_CT_MP_MISSIONS),		// PRESENCE_PRES_7
	MakeFunctorRet(&EventWriteJA_CT_SPMG),				// PRESENCE_PRES_8
	MakeFunctorRet(&EventWriteJA_CT_SPRC),				// PRESENCE_PRES_9
	MakeFunctorRet(&events_durango::NoOpPresence)	// PRESENCE_PRES_10
};

// Active Presence Contexts for TwitchTV Xbox One App
Functor3Ret<ULONG, PCWSTR, LPCGUID, PCWSTR> events_durango::m_fActiveContext[XB1_PRESENCE_CONTEXT_EVENTS] =
{
	MakeFunctorRet(&events_durango::NoOpPresence),	// PRESENCE_PRES_0
	MakeFunctorRet(&EventWriteAS_CT_SP_MISSIONS),	// PRESENCE_PRES_1
	MakeFunctorRet(&events_durango::NoOpPresence),	// PRESENCE_PRES_2
	MakeFunctorRet(&events_durango::NoOpPresence),	// PRESENCE_PRES_3
	MakeFunctorRet(&events_durango::NoOpPresence),	// PRESENCE_PRES_4
	MakeFunctorRet(&EventWriteAS_CT_DEATHM_LOC),	// PRESENCE_PRES_5
	MakeFunctorRet(&events_durango::NoOpPresence),	// PRESENCE_PRES_6
	MakeFunctorRet(&EventWriteAS_CT_MP_MISSIONS),	// PRESENCE_PRES_7
	MakeFunctorRet(&EventWriteAS_CT_SPMG),			// PRESENCE_PRES_8
	MakeFunctorRet(&EventWriteAS_CT_SPRC),			// PRESENCE_PRES_9
	MakeFunctorRet(&events_durango::NoOpPresence)	// PRESENCE_PRES_10
};


bool events_durango::sm_Initialized = false;
events_durango::EventRegistrationWorker events_durango::sm_Worker;
u64 events_durango::m_LastArchenemy = 0;
int events_durango::sm_iPreviousAchievementProgress[XB1_ACHIEVEMENT_EVENTS];

bool events_durango::Init()
{
	g_rlXbl.GetAchivementManager()->SetAchievementEventFunc(MakeFunctorRet(&WriteAchievementEvent));

	// reset previous achievement progress array
	for (int i = 0; i < XB1_ACHIEVEMENT_EVENTS; i++)
		sm_iPreviousAchievementProgress[i] = -1;

	bool success = false;

	rtry
	{
		rverify(!sm_Initialized, catchall,);
		rverify(sm_Worker.Start(1), catchall, gnetError("Error starting event registration worker"));

		sm_Worker.Wakeup();

		gnetDebug1("Started event registration worker");

		success = sm_Initialized = true;
	}
	rcatchall
	{
	}

	return success;
}

bool events_durango::Shutdown()
{
	gnetDebug1("Events_Durango is shutting down.");
	ULONG result = 0;
	if (sysAppContent::IsJapaneseBuild())
	{
		result = EventUnregisterRKTR_3E961D5B();
	}
	else
	{
		result = EventUnregisterRKTR_39F35803();
	}
	return result == ERROR_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
// rlAchievement::Worker
///////////////////////////////////////////////////////////////////////////////

events_durango::EventRegistrationWorker::EventRegistrationWorker()
{
}

bool events_durango::EventRegistrationWorker::Start(const int cpuAffinity)
{
	const bool success = this->rlWorker::Start("[GTAV] Event Registration", sysIpcMinThreadStackSize, cpuAffinity, true);
	return success;
}

bool events_durango::EventRegistrationWorker::Stop()
{
	const bool success = this->rlWorker::Stop();
	return success;
}

void events_durango::EventRegistrationWorker::Perform()
{
	if (sysAppContent::IsJapaneseBuild())
	{
		result = EventRegisterRKTR_3E961D5B();
	}
	else
	{
		result = EventRegisterRKTR_39F35803();
	}

	gnetDebug1("EventRegistrationWorker, response: %lu", result);
}

bool events_durango::WriteAchievementEvent(int achId, int localGamerIndex, int achievementProgress)
{
	rtry
	{
		rcheck(RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex), catchall, gnetError("events_durango invalid local gamer index"));
		rcheck(sm_Initialized, catchall, gnetError("events_durango not initialized"));
		rcheck(achId >= 0 && achId < XB1_ACHIEVEMENT_EVENTS, catchall, gnetError("Invalid achievement ID passed to WriteAchievementEvent"));

		PCWSTR playerId = GetUserId(localGamerIndex);
		GUID playerSessionId;
		ZeroMemory( &playerSessionId, sizeof(GUID) );

		int progress = achievementProgress;
		int threshold = m_iAchievementThresholds[achId];
		if (achievementProgress == IAchievements::FULL_UNLOCK_PROGRESS)
		{
			gnetDebug1("FULL_UNLOCK_PROGRESS(-1) is set, setting progress from %d to %d", achievementProgress, threshold);
			progress = threshold;
		}

		rcheck(sm_iPreviousAchievementProgress[achId] != progress, catchall, gnetError("Trying to write achievement progress %d for ID %d despite no change", progress, achId));
		sm_iPreviousAchievementProgress[achId] = progress;

		ULONG response;
		if (sysAppContent::IsJapaneseBuild())
		{
			response = m_fAchievementEventsJA[achId](playerId, &playerSessionId, progress);
		}
		else
		{
			response = m_fAchievementEvents[achId](playerId, &playerSessionId, progress);
		}
		
		if (response == ERROR_SUCCESS)
		{
			gnetDebug1("WriteAchievementEvent, id: %d, progress: %d of threshold: %d, response: %lu", achId, progress, threshold, response);
			return true;
		}

		gnetDebug1("WriteAchievementEvent, id: %d, progress: %d of threshold: %d, error: %lu", achId, progress, threshold, response);
	}
	rcatchall
	{
		gnetError("Failed to write achievement event for ID %d", achId);
	}

	return false;
}

bool events_durango::WritePresenceContextEvent(const rlGamerInfo& gamerInfo, const richPresenceFieldStat* stats, int numStats)
{
	bool bSuccess = false;

	rtry
	{
		rcheck(sm_Initialized, catchall, gnetError("events_durango not initialized"));
		rverify(gamerInfo.IsValid(), catchall, );
		rverify(stats, catchall, );

		PCWSTR playerId = GetUserId(gamerInfo.GetLocalIndex());
		GUID playerSessionId;
		ZeroMemory( &playerSessionId, sizeof(GUID) );

		for (int i = 0; i < numStats; i++)
		{			
			int presenceId = stats[i].m_PresenceId;
			rverify(presenceId < XB1_PRESENCE_CONTEXT_EVENTS && presenceId >= 0
				, catchall
				, gnetError("Invalid presenceId=%d, Max value=%d", presenceId, XB1_PRESENCE_CONTEXT_EVENTS));

			wchar_t wStatAttr[RL_PRESENCE_MAX_STAT_SIZE];
			Utf8ToWide((char16*)wStatAttr, stats[i].m_FieldStat, RL_PRESENCE_MAX_STAT_SIZE);

			ULONG response;
			if (sysAppContent::IsJapaneseBuild())
			{
				response = m_fPresenceContextEventsJA[presenceId](playerId, &playerSessionId, wStatAttr);
			}
			else
			{
				response = m_fPresenceContextEvents[presenceId](playerId, &playerSessionId, wStatAttr);
			}
			rverify(response == ERROR_SUCCESS, catchall, );
			gnetDebug1("WritePresenceContextEvent, response: %lu", response);
		}

		// Write the active context for the XBO Twitch TV App
		if (!sysAppContent::IsJapaneseBuild())
		{
			WriteActiveContext(gamerInfo, stats, numStats);
		}

		bSuccess = true;
	}
	rcatchall
	{

	}

	return bSuccess;
}

void events_durango::WriteActiveContext(const rlGamerInfo& gamerInfo, const richPresenceFieldStat* stats, int numStats)
{
	PCWSTR playerId = GetUserId(gamerInfo.GetLocalIndex());
	GUID playerSessionId;
	ZeroMemory( &playerSessionId, sizeof(GUID) );

	for (int i = 0; i < numStats; i++)
	{			
		rtry
		{
			// Microsoft has asked us to clear our other stats when writing a stat event. This would allow for
			// rich applications like Twitch to determine which presence context is the active one. We iterate
			// from 0 to XB1_PRESENCE_CONTEXT_EVENTS, and for each event that isn't the corresponding presenceId,
			// simply write an empty string.
			int presenceId = stats[i].m_PresenceId;

			rverify(presenceId < XB1_PRESENCE_CONTEXT_EVENTS && presenceId >= 0
				, catchall
				, gnetError("Invalid presenceId=%d, Max value=%d", presenceId, XB1_PRESENCE_CONTEXT_EVENTS));

			for (int j = 0; j < XB1_PRESENCE_CONTEXT_EVENTS; j++)
			{
				wchar_t wStatAttr[RL_PRESENCE_MAX_STAT_SIZE] = {0};
				if (presenceId == j)
				{
					Utf8ToWide((char16*)wStatAttr, stats[i].m_FieldStat, RL_PRESENCE_MAX_STAT_SIZE);
				}

				ULONG response = m_fActiveContext[j](playerId, &playerSessionId, wStatAttr);
				rverify(response == ERROR_SUCCESS, catchall, );
			}
		}
		rcatchall
		{

		}
	}
}

PCWSTR events_durango::GetUserId(int localGamerIndex)
{
	User^ actingUser = rlXblInternal::GetInstance()->_GetPresenceManager()->GetXboxUser(localGamerIndex);
	if (actingUser != nullptr && actingUser->XboxUserId != nullptr)
	{
		return actingUser->XboxUserId->Data();
	}
	
	return NULL;
}

LPCGUID events_durango::GetPlayerSessionId()
{
	// TODO JRM: Populate an ID here if we want to leverage xb1 stats
	return 0;
}

PCWSTR events_durango::GetMultiplayerCorrelationId()
{
	// TODO JRM: Populate an ID here if we want to leverage xb1 stats
	return L"";
}

int events_durango::GetGameplayModeId()
{
	// TODO JRM: Populate an ID here if we want to leverage xb1 stats
	return 0;
}

int events_durango::GetSectionId()
{
	// TODO JRM: Populate an ID here if we want to leverage xb1 stats
	return 0;
}

int events_durango::GetDifficultyLevelId()
{
	// TODO JRM: Populate an ID here if we want to leverage xb1 stats
	return 0;
}

int events_durango::GetPlayerRoleId()
{
	// TODO JRM: Populate an ID here if we want to leverage xb1 stats
	return 0;
}

int events_durango::GetPlayerWeaponId()
{
	// TODO JRM: Populate an ID here if we want to leverage xb1 stats
	return 0;
}

int events_durango::GetEnemyRoleId()
{
	// TODO JRM: Populate an ID here if we want to leverage xb1 stats
	return 0;
}

int events_durango::GetKillTypeId()
{
	// TODO JRM: Populate an ID here if we want to leverage xb1 stats
	return 0;
}

Vec3V_Out events_durango::GetPlayerLocation()
{
	CPed * pPlayer = FindPlayerPed();
	if(pPlayer)
	{
		return pPlayer->GetTransform().GetPosition();
	}

	return Vec3V_Out(0.0f, 0.0f, 0.0f);
}

int events_durango::GetEnemyWeaponId()
{
	// TODO JRM: Populate an ID here if we want to leverage xb1 stats
	return 0;
}

int events_durango::GetMatchTypeId()
{
	// TODO JRM: Populate an ID here if we want to leverage xb1 stats
	return 0;
}

int events_durango::GetMatchTimeInSeconds()
{
	// TODO JRM: Populate an ID here if we want to leverage xb1 stats
	return 0;
}

int events_durango::GetExitStatusId()
{
	// TODO JRM: Populate an ID here if we want to leverage xb1 stats
	return 0;
}

bool events_durango::WriteEvent_EnemyDefeatedSP(int totalKills)
{
	rtry
	{
		rcheck(totalKills > 0, catchall, );
		int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
		rcheck(RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex), catchall, );

		PCWSTR playerId = GetUserId(localGamerIndex);

		GUID playerSessionId;
		ZeroMemory( &playerSessionId, sizeof(GUID) );

		PCWSTR multiplayerCorrelationId = GetMultiplayerCorrelationId();
		int sectionId = GetSectionId();
		int gamePlayModeId = GetGameplayModeId();
		int difficultyLevel = GetDifficultyLevelId();
		int playerRoleId = GetPlayerRoleId();

		GUID roundId;
		ZeroMemory( &roundId, sizeof(GUID) );

		Vec3V_Out playerPos = GetPlayerLocation();

		int playerWeaponId = GetPlayerWeaponId();
		int enemyRoleId = GetEnemyRoleId();
		int killTypeId = GetKillTypeId();
		int enemyWeaponId = GetEnemyWeaponId();

		ULONG result;
		if (sysAppContent::IsJapaneseBuild())
		{
			result = EventWriteJA_EnemyDefeatedSP(playerId, sectionId, &playerSessionId, multiplayerCorrelationId, gamePlayModeId, difficultyLevel, &roundId, playerRoleId, playerWeaponId, enemyRoleId, killTypeId, playerPos.GetX().Getf(), playerPos.GetY().Getf(), playerPos.GetZ().Getf(), enemyWeaponId);
		}
		else
		{
			result = EventWriteEnemyDefeatedSP(playerId, sectionId, &playerSessionId, multiplayerCorrelationId, gamePlayModeId, difficultyLevel, &roundId, playerRoleId, playerWeaponId, enemyRoleId, killTypeId, playerPos.GetX().Getf(), playerPos.GetY().Getf(), playerPos.GetZ().Getf(), enemyWeaponId);
		}

		gnetDebug1("WriteEvent_EnemyDefeatedSP, response: %lu", result);
		return result == ERROR_SUCCESS;
	}
	rcatchall
	{

	}

	return false;
}

bool events_durango::WriteEvent_HeldUpShops(int helpUpShops)
{
	rtry
	{
		int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
		rcheck(RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex), catchall, );

		PCWSTR playerId = GetUserId(localGamerIndex);

		GUID playerSessionId;
		ZeroMemory( &playerSessionId, sizeof(GUID) );

		ULONG result;
		if (sysAppContent::IsJapaneseBuild())
		{
			result = EventWriteJA_HeldUpShops(playerId, &playerSessionId, helpUpShops);
		}
		else
		{
			result = EventWriteHeldUpShops(playerId, &playerSessionId, helpUpShops);
		}

		gnetDebug1("WriteEvent_HeldUpShops, response: %lu", result);
		return result == ERROR_SUCCESS;
	}
	rcatchall
	{

	}

	return false;
}

bool events_durango::WriteEvent_HordeWavesSurvived(int hordeWavesSurvived)
{
	rtry
	{
		rcheck(hordeWavesSurvived > 0, catchall, ); // don't write on stat clear

		int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
		rcheck(RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex), catchall, );

		PCWSTR playerId = GetUserId(localGamerIndex);

		GUID playerSessionId;
		ZeroMemory( &playerSessionId, sizeof(GUID) );

		ULONG result;
		if (sysAppContent::IsJapaneseBuild())
		{
			result = EventWriteJA_HordeWavesSurvived(playerId, &playerSessionId, hordeWavesSurvived);
		}
		else
		{
			result = EventWriteHordeWavesSurvived(playerId, &playerSessionId, hordeWavesSurvived);
		}
		
		gnetDebug1("WriteEvent_HordeWavesSurvived, response: %lu", result);
		return result == ERROR_SUCCESS;
	}
	rcatchall
	{

	}

	return false;
}

bool events_durango::WriteEvent_MoneyTotalSpent(int character, int totalMoneySpent)
{
	rtry
	{
		rcheck(totalMoneySpent > 0, catchall, ); // don't write on stat clear

		int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
		rcheck(RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex), catchall, );

		PCWSTR playerId = GetUserId(localGamerIndex);

		GUID playerSessionId;
		ZeroMemory( &playerSessionId, sizeof(GUID) );

		ULONG result;
		if (sysAppContent::IsJapaneseBuild())
		{
			result = EventWriteJA_MoneyTotalSpent(playerId, &playerSessionId, character, totalMoneySpent);
		}
		else
		{
			result = EventWriteMoneyTotalSpent(playerId, &playerSessionId, character, totalMoneySpent);
		}

		gnetDebug1("WriteEvent_MoneyTotalSpent, response: %lu", result);
		return result == ERROR_SUCCESS;
	}
	rcatchall
	{

	}

	return false;
}

bool events_durango::WriteEvent_MultiplayerRoundStart(const snSession* pSession, int nGameplayModeId, MatchType kMatchType)
{
	// an ID provided by the multiplayer session directory (MPSD) service used to correlate this event with the current multiplayer session
	String^ rtMultiplayerCorrelationId = rlXblInternal::GetInstance()->_GetSessionManager()->GetMultiplayerCorrelationId(&pSession->GetSessionInfo().GetSessionHandle());
	PCWSTR multiplayerCorrelationId = rtMultiplayerCorrelationId->Data();

	// an ID generated by the title that is used to correlate events that occur in a single player session. 
	// it is used to analyze behavior for a player within a specific gameplay session.
	// session name is unique so we'll use that
	GUID playerSessionId;
	sysMemCpy(&playerSessionId, pSession->GetSessionInfo().GetSessionHandle().m_SessionName.GetGuid(), sizeof(playerSessionId));

	return WriteEvent_MultiplayerRoundStart(multiplayerCorrelationId, playerSessionId, nGameplayModeId, kMatchType);
}

bool events_durango::WriteEvent_MultiplayerRoundStart(PCWSTR correlationId, const GUID& playerSessionId, int nGameplayModeId, MatchType kMatchType)
{
	rtry
	{
		int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
		rcheck(RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex), catchall, );
		rcheck(CLiveManager::IsOnline(), catchall, );

		// XUID for the local player
		PCWSTR playerId = GetUserId(localGamerIndex);

		// we could count consecutive activities here
		GUID roundId;
		ZeroMemory(&roundId, sizeof(GUID));

		// narrative section - pass in whether someone has completed the multiplayer tutorial or not 
		int sectionId = 0;

		// an in-game setting that significantly differentiates the play style of the game.
		int gamePlayModeId = nGameplayModeId;

		// use this field to identify distinct types of matchmaking supported by the title (one of MatchType)
		int matchTypeId = static_cast<int>(kMatchType);

		// used to identify the difficulty level the user is currently playing on
		int difficultyLevelId = 0;

		ULONG result;
		if (sysAppContent::IsJapaneseBuild())
		{
			result = EventWriteJA_MultiplayerRoundStart(playerId, &roundId, sectionId, &playerSessionId, correlationId, gamePlayModeId, matchTypeId, difficultyLevelId);
		}
		else
		{
			result = EventWriteMultiplayerRoundStart(playerId, &roundId, sectionId, &playerSessionId, correlationId, gamePlayModeId, matchTypeId, difficultyLevelId);
		}
		
		gnetDebug1("WriteEvent_MultiplayerRoundStart :: CorrelationId: %ls, Response: %lu", correlationId, result);
		return result == ERROR_SUCCESS;
	}
	rcatchall
	{

	}

	return false;
}

bool events_durango::WriteEvent_MultiplayerRoundEnd(const snSession* pSession, int nGameplayModeId, MatchType kMatchType, unsigned nTimeInMs, ExitStatus kExitStatus)
{
	// an ID provided by the multiplayer session directory (MPSD) service used to correlate this event with the current multiplayer session
	String^ rtMultiplayerCorrelationId = rlXblInternal::GetInstance()->_GetSessionManager()->GetMultiplayerCorrelationId(&pSession->GetSessionInfo().GetSessionHandle());
	PCWSTR multiplayerCorrelationId = rtMultiplayerCorrelationId->Data();

	// an ID generated by the title that is used to correlate events that occur in a single player session. 
	// it is used to analyze behavior for a player within a specific gameplay session.
	// session name is unique so we'll use that
	GUID playerSessionId;
	sysMemCpy(&playerSessionId, pSession->GetSessionInfo().GetSessionHandle().m_SessionName.GetGuid(), sizeof(playerSessionId));

	return WriteEvent_MultiplayerRoundEnd(multiplayerCorrelationId, playerSessionId, nGameplayModeId, kMatchType, nTimeInMs, kExitStatus);
}

bool events_durango::WriteEvent_MultiplayerRoundEnd(PCWSTR correlationId, const GUID& playerSessionId, int nGameplayModeId, MatchType kMatchType, unsigned nTimeInMs, ExitStatus kExitStatus)
{
	rtry
	{
		int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
		rcheck(RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex), catchall, );
		rcheck(CLiveManager::IsOnline(), catchall, );

		// XUID for the local player
		PCWSTR playerId = GetUserId(localGamerIndex);

		// we could count consecutive activities here
		GUID roundId;
		ZeroMemory(&roundId, sizeof(GUID));

		// narrative section - pass in whether someone has completed the multiplayer tutorial or not 
		int sectionId = 0;

		// an in-game setting that significantly differentiates the play style of the game.
		int gamePlayModeId = nGameplayModeId;

		// use this field to identify distinct types of matchmaking supported by the title (one of MatchType)
		int matchTypeId = static_cast<int>(kMatchType);

		// used to identify the difficulty level the user is currently playing on
		int difficultyLevelId = 0;

		// this field is used to track the amount of time the user has spent playing a section or completing an in-game activity.
		float timeInSeconds = static_cast<float>(nTimeInMs / 1000);

		// this field is used to identify the result or circumstances in which a user exited a section or multiplayer round in a game (one of ExitStatus)
		int exitStatusId = static_cast<int>(kExitStatus);

		ULONG result;
		if (sysAppContent::IsJapaneseBuild())
		{
			result = EventWriteJA_MultiplayerRoundEnd(playerId, &roundId, sectionId, &playerSessionId, correlationId, gamePlayModeId, matchTypeId, difficultyLevelId, timeInSeconds, exitStatusId);
		}
		else
		{
			result = EventWriteMultiplayerRoundEnd(playerId, &roundId, sectionId, &playerSessionId, correlationId, gamePlayModeId, matchTypeId, difficultyLevelId, timeInSeconds, exitStatusId);
		}
		gnetDebug1("WriteEvent_MultiplayerRoundEnd :: CorrelationId: %ls, Response: %lu", correlationId, result);
		return result == ERROR_SUCCESS;
	}
	rcatchall
	{

	}

	return false;
}

bool events_durango::WriteEvent_PlatinumAwards(int numPlatinumAwards)
{
	rtry
	{
		rcheck(numPlatinumAwards > 0, catchall, ); // don't write on stat clear

		int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
		rcheck(RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex), catchall, );

		PCWSTR playerId = GetUserId(localGamerIndex);

		GUID playerSessionId;
		ZeroMemory( &playerSessionId, sizeof(GUID) );

		ULONG result;
		if (sysAppContent::IsJapaneseBuild())
		{
			result = EventWriteJA_PlatinumAwards(playerId, &playerSessionId, numPlatinumAwards);
		}
		else
		{
			result = EventWritePlatinumAwards(playerId, &playerSessionId, numPlatinumAwards);
		}
		
		gnetDebug1("WriteEvent_PlatinumAwards, response: %lu", result);
		return result == ERROR_SUCCESS;
	}
	rcatchall
	{

	}

	return false;
}

bool events_durango::WriteEvent_PlayerDefeatedSP(int totalKills)
{
	rtry
	{
		rcheck(totalKills > 0, catchall, );
		int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
		rcheck(RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex), catchall, );

		PCWSTR playerId = GetUserId(localGamerIndex);

		GUID playerSessionId;
		ZeroMemory( &playerSessionId, sizeof(GUID) );

		PCWSTR multiplayerCorrelationId = GetMultiplayerCorrelationId();
		int sectionId = GetSectionId();
		int gamePlayModeId = GetGameplayModeId();
		int difficultyLevel = GetDifficultyLevelId();
		int playerRoleId = GetPlayerRoleId();

		LPCGUID roundId;
		ZeroMemory( &roundId, sizeof(LPCGUID) );

		Vec3V_Out playerPos = GetPlayerLocation();

		int playerWeaponId = GetPlayerWeaponId();
		int enemyRoleId = GetEnemyRoleId();
		int enemyWeaponId = GetEnemyWeaponId();

		ULONG result;
		if (sysAppContent::IsJapaneseBuild())
		{
			result = EventWriteJA_PlayerDefeatedSP(playerId, sectionId, &playerSessionId, multiplayerCorrelationId, gamePlayModeId, difficultyLevel, roundId, playerRoleId, playerWeaponId, enemyRoleId, enemyWeaponId, playerPos.GetX().Getf(), playerPos.GetY().Getf(), playerPos.GetZ().Getf());
		}
		else
		{
			result = EventWritePlayerDefeatedSP(playerId, sectionId, &playerSessionId, multiplayerCorrelationId, gamePlayModeId, difficultyLevel, roundId, playerRoleId, playerWeaponId, enemyRoleId, enemyWeaponId, playerPos.GetX().Getf(), playerPos.GetY().Getf(), playerPos.GetZ().Getf());
		}

		gnetDebug1("WriteEvent_PlayerDefeatedSP, response: %lu", result);
		return result == ERROR_SUCCESS;
	}
	rcatchall
	{

	}

	return false;
}

bool events_durango::WriteEvent_PlayerKillsMP(int totalKills)
{
	rtry
	{
		rcheck(totalKills > 0, catchall, );
		int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
		rcheck(RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex), catchall, );

		PCWSTR playerId = GetUserId(localGamerIndex);

		GUID playerSessionId;
		ZeroMemory( &playerSessionId, sizeof(GUID) );

		PCWSTR multiplayerCorrelationId = GetMultiplayerCorrelationId();
		int sectionId = GetSectionId();
		int gamePlayModeId = GetGameplayModeId();
		int difficultyLevel = GetDifficultyLevelId();
		int playerRoleId = GetPlayerRoleId();

		LPCGUID roundId;
		ZeroMemory( &roundId, sizeof(LPCGUID) );

		Vec3V_Out playerPos = GetPlayerLocation();

		int playerWeaponId = GetPlayerWeaponId();
		int enemyRoleId = GetEnemyRoleId();
		int killTypeId = GetKillTypeId();
		int enemyWeaponId = GetEnemyWeaponId();

		ULONG result;
		if (sysAppContent::IsJapaneseBuild())
		{
			result = EventWriteJA_PlayerKillsMP(playerId, sectionId, &playerSessionId, multiplayerCorrelationId, gamePlayModeId, difficultyLevel, roundId, playerRoleId, playerWeaponId, enemyRoleId, killTypeId, playerPos.GetX().Getf(), playerPos.GetY().Getf(), playerPos.GetZ().Getf(), enemyWeaponId);
		}
		else
		{
			result = EventWritePlayerKillsMP(playerId, sectionId, &playerSessionId, multiplayerCorrelationId, gamePlayModeId, difficultyLevel, roundId, playerRoleId, playerWeaponId, enemyRoleId, killTypeId, playerPos.GetX().Getf(), playerPos.GetY().Getf(), playerPos.GetZ().Getf(), enemyWeaponId);
		}

		gnetDebug1("WriteEvent_PlayerKillsMP, response: %lu", result);
		return result == ERROR_SUCCESS;
	}
	rcatchall
	{

	}

	return false;
}

bool events_durango::WriteEvent_PlayerSessionEnd()
{
	rtry
	{
		int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
		rcheck(RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex), catchall, );

		PCWSTR playerId = GetUserId(localGamerIndex);

		GUID playerSessionId;
		ZeroMemory( &playerSessionId, sizeof(GUID) );

		PCWSTR multiplayerCorrelationId = GetMultiplayerCorrelationId();
		int gamePlayModeId = GetGameplayModeId();
		int difficultyLevel = GetDifficultyLevelId();
		int exitStatusId = GetExitStatusId();

		ULONG result;
		if (sysAppContent::IsJapaneseBuild())
		{
			result = EventWriteJA_PlayerSessionEnd(playerId, &playerSessionId, multiplayerCorrelationId, gamePlayModeId, difficultyLevel, exitStatusId);
		}
		else
		{
			result = EventWritePlayerSessionEnd(playerId, &playerSessionId, multiplayerCorrelationId, gamePlayModeId, difficultyLevel, exitStatusId);
		}

		gnetDebug1("WriteEvent_PlayerSessionEnd, response: %lu", result);
		return result == ERROR_SUCCESS;
	}
	rcatchall
	{

	}

	return false;
}

bool events_durango::WriteEvent_PlayerSessionPause()
{
	rtry
	{
		int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
		rcheck(RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex), catchall, );

		PCWSTR playerId = GetUserId(localGamerIndex);

		GUID playerSessionId;
		ZeroMemory( &playerSessionId, sizeof(GUID) );

		PCWSTR multiplayerCorrelationId = GetMultiplayerCorrelationId();

		ULONG result;
		if (sysAppContent::IsJapaneseBuild())
		{
			result = EventWriteJA_PlayerSessionPause(playerId, &playerSessionId, multiplayerCorrelationId);
		}
		else
		{
			result = EventWritePlayerSessionPause(playerId, &playerSessionId, multiplayerCorrelationId);
		}

		gnetDebug1("WriteEvent_PlayerSessionPause for %ls, response: %lu", playerId, result);
		return result == ERROR_SUCCESS;
	}
	rcatchall
	{

	}

	return false;
}

bool events_durango::WriteEvent_PlayerSessionResume()
{
	rtry
	{
		int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
		rcheck(RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex), catchall, );

		PCWSTR playerId = GetUserId(localGamerIndex);

		GUID playerSessionId;
		ZeroMemory( &playerSessionId, sizeof(GUID) );

		PCWSTR multiplayerCorrelationId = GetMultiplayerCorrelationId();
		int gamePlayModeId = GetGameplayModeId();
		int difficultyLevel = GetDifficultyLevelId();

		ULONG result;
		if (sysAppContent::IsJapaneseBuild())
		{
			result = EventWriteJA_PlayerSessionResume(playerId, &playerSessionId, multiplayerCorrelationId, gamePlayModeId, difficultyLevel);
		}
		else
		{
			result = EventWritePlayerSessionResume(playerId, &playerSessionId, multiplayerCorrelationId, gamePlayModeId, difficultyLevel);
		}

		gnetDebug1("WriteEvent_PlayerSessionResume, response: %lu", result);
		return result == ERROR_SUCCESS;
	}
	rcatchall
	{

	}

	return false;
}

bool events_durango::WriteEvent_PlayerSessionStart()
{
	rtry
	{
		int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
		rcheck(RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex), catchall, );

		PCWSTR playerId = GetUserId(localGamerIndex);

		GUID playerSessionId;
		ZeroMemory( &playerSessionId, sizeof(GUID) );

		PCWSTR multiplayerCorrelationId = GetMultiplayerCorrelationId();
		int gamePlayModeId = GetGameplayModeId();
		int difficultyLevel = GetDifficultyLevelId();

		ULONG result;
		if (sysAppContent::IsJapaneseBuild())
		{
			result = EventWriteJA_PlayerSessionStart(playerId, &playerSessionId, multiplayerCorrelationId, gamePlayModeId, difficultyLevel);
		}
		else
		{
			result = EventWritePlayerSessionStart(playerId, &playerSessionId, multiplayerCorrelationId, gamePlayModeId, difficultyLevel);
		}

		gnetDebug1("WriteEvent_PlayerSessionStart, response: %lu", result);
		return result == ERROR_SUCCESS;
	}
	rcatchall
	{

	}

	return false;
}

bool events_durango::WriteEvent_PlayerRespawnedSP()
{
	rtry
	{
		int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
		rcheck(RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex), catchall, );

		PCWSTR playerId = GetUserId(localGamerIndex);

		GUID playerSessionId;
		ZeroMemory( &playerSessionId, sizeof(GUID) );

		PCWSTR multiplayerCorrelationId = GetMultiplayerCorrelationId();
		int sectionId = GetSectionId();
		int gamePlayModeId = GetGameplayModeId();
		int difficultyLevel = GetDifficultyLevelId();
		int playerRoleId = GetPlayerRoleId();

		GUID roundId;
		ZeroMemory( &roundId, sizeof(roundId) );

		Vec3V_Out playerPos = GetPlayerLocation();

		ULONG result;
		if (sysAppContent::IsJapaneseBuild())
		{
			result = EventWriteJA_PlayerSpawnedSP(playerId, sectionId, &playerSessionId, multiplayerCorrelationId, gamePlayModeId, difficultyLevel, &roundId, playerRoleId, playerPos.GetX().Getf(), playerPos.GetY().Getf(), playerPos.GetZ().Getf());
		}
		else
		{
			result = EventWritePlayerSpawnedSP(playerId, sectionId, &playerSessionId, multiplayerCorrelationId, gamePlayModeId, difficultyLevel, &roundId, playerRoleId, playerPos.GetX().Getf(), playerPos.GetY().Getf(), playerPos.GetZ().Getf());
		}

		gnetDebug1("WriteEvent_PlayerRespawnedSP, response: %lu", result);
		return result == ERROR_SUCCESS;
	}
	rcatchall
	{

	}

	return false;
}

bool events_durango::WriteEvent_TotalCustomRacesWon(int totalRacesWon)
{
	rtry
	{
		rcheck(totalRacesWon > 0, catchall, ); // don't write on stat clear

		int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
		rcheck(RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex), catchall, );

		PCWSTR playerId = GetUserId(localGamerIndex);

		GUID playerSessionId;
		ZeroMemory( &playerSessionId, sizeof(GUID) );

		ULONG result;
		if (sysAppContent::IsJapaneseBuild())
		{
			result = EventWriteJA_TotalCustomRacesWon(playerId, &playerSessionId, totalRacesWon);
		}
		else
		{
			result = EventWriteTotalCustomRacesWon(playerId, &playerSessionId, totalRacesWon);
		}

		gnetDebug1("WriteEvent_TotalCustomRacesWon val=%d, response: %lu", totalRacesWon, result);
		return result == ERROR_SUCCESS;
	}
	rcatchall
	{

	}

	return false;
}

bool events_durango::WriteEvent_GameProgressSP(float gameProgress)
{
	rtry
	{
		rcheck(gameProgress > 0, catchall, ); // don't write on stat clear

		int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
		rcheck(RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex), catchall, );

		PCWSTR playerId = GetUserId(localGamerIndex);

		GUID playerSessionId;
		ZeroMemory( &playerSessionId, sizeof(GUID) );

		float progress = round(gameProgress);

		// needed for hero stat: Total Progress Made In GTA V
		ULONG result;
		if (sysAppContent::IsJapaneseBuild())
		{
			result = EventWriteJA_TOTAL_PROGRESS_MADE(playerId, &playerSessionId, progress, gameProgress);
		}
		else
		{
			result = EventWriteTOTAL_PROGRESS_MADE(playerId, &playerSessionId, progress, gameProgress);
		}

		gnetDebug1("EventWriteTOTAL_PROGRESS_MADE progress=%f, gameProgress=%f, response: %lu", progress, gameProgress, result);
		return result == ERROR_SUCCESS;
	}
	rcatchall
	{

	}

	return false;
}

bool events_durango::WriteEvent_PlayingTime(u64 nTimeInMs)
{
	rtry
	{
		rcheck(nTimeInMs > 0, catchall, ); // don't write on stat clear

		int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
		rcheck(RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex), catchall, );

		PCWSTR playerId = GetUserId(localGamerIndex);

		GUID playerSessionId;
		ZeroMemory( &playerSessionId, sizeof(GUID) );

		const signed int minutesPlayed = static_cast< const signed int >(nTimeInMs/1000/60);

		ULONG result;
		if (sysAppContent::IsJapaneseBuild())
		{
			result = EventWriteJA_SPMinutesPlayed(playerId, &playerSessionId, minutesPlayed);
		}
		else
		{
			result = EventWriteSPMinutesPlayed(playerId, &playerSessionId, minutesPlayed);
		}

		gnetDebug1("EventWriteSPMinutesPlayed val=%" I64FMT "u : '%dm', response: %lu", nTimeInMs, minutesPlayed, result);
		return result == ERROR_SUCCESS;
	}
	rcatchall
	{

	}

	return false;
}

bool events_durango::WriteEvent_CashEarnedSP(s64 cashEarned)
{
	rtry
	{
		rcheck(cashEarned > 0, catchall, ); // don't write on stat clear

		int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
		rcheck(RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex), catchall, );

		PCWSTR playerId = GetUserId(localGamerIndex);

		GUID playerSessionId;
		ZeroMemory( &playerSessionId, sizeof(GUID) );

		char cashBuff[32];
		CFrontendStatsMgr::FormatInt64ToCash(cashEarned, cashBuff, sizeof(cashBuff)/sizeof(cashBuff[0]));

		wchar_t szStringCash[RL_MAX_DISPLAY_NAME_BUF_SIZE];
		String^ stringCash = rlXblCommon::UTF8ToWinRtString(cashBuff, szStringCash, RL_MAX_DISPLAY_NAME_BUF_SIZE);

		ULONG result;
		if (sysAppContent::IsJapaneseBuild())
		{
			result = EventWriteJA_TOTAL_CASH_EARNED(playerId, &playerSessionId, stringCash->Data());
		}
		else
		{
			result = EventWriteTOTAL_CASH_EARNED(playerId, &playerSessionId, stringCash->Data());
		}

		gnetDebug1("EventWriteTOTAL_CASH_EARNED val=%d,'%s', response: %lu", cashEarned, cashBuff, result);
		return result == ERROR_SUCCESS;
	}
	rcatchall
	{

	}

	return false;
}

bool events_durango::WriteEvent_StolenCars(unsigned stolenCars)
{
	rtry
	{
		rcheck(stolenCars > 0, catchall, ); // don't write on stat clear

		int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
		rcheck(RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex), catchall, );

		PCWSTR playerId = GetUserId(localGamerIndex);

		GUID playerSessionId;
		ZeroMemory( &playerSessionId, sizeof(GUID) );

		ULONG result;
		if (sysAppContent::IsJapaneseBuild())
		{
			result = EventWriteJA_NUMBER_STOLEN_CARS(playerId, &playerSessionId, stolenCars);
		}
		else
		{
			result = EventWriteNUMBER_STOLEN_CARS(playerId, &playerSessionId, stolenCars);
		}

		gnetDebug1("EventWriteNUMBER_STOLEN_CARS val=%u, response: %lu", stolenCars, result);
		return result == ERROR_SUCCESS;
	}
	rcatchall
	{

	}

	return false;
}

bool events_durango::WriteEvent_StarsAttained(unsigned starsAttained)
{
	rtry
	{
		rcheck(starsAttained > 0, catchall, ); // don't write on stat clear

		int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
		rcheck(RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex), catchall, );

		PCWSTR playerId = GetUserId(localGamerIndex);

		GUID playerSessionId;
		ZeroMemory( &playerSessionId, sizeof(GUID) );

		ULONG result;
		if (sysAppContent::IsJapaneseBuild())
		{
			result = EventWriteJA_STARS_ATTAINED(playerId, &playerSessionId, starsAttained);
		}
		else
		{
			result = EventWriteSTARS_ATTAINED(playerId, &playerSessionId, starsAttained);
		}

		gnetDebug1("EventWriteSTARS_ATTAINED val=%u, response: %lu", starsAttained, result);
		return result == ERROR_SUCCESS;
	}
	rcatchall
	{

	}

	return false;
}

bool events_durango::WriteEvent_MultiplayerPlaytime(u64 nTimeInMs)
{
	rtry
	{
		rcheck(nTimeInMs > 0, catchall, ); // don't write on stat clear

		int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
		rcheck(RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex), catchall, );

		PCWSTR playerId = GetUserId(localGamerIndex);

		GUID playerSessionId;
		ZeroMemory( &playerSessionId, sizeof(GUID) );

		const u32 c_stringSize = 128;
		char szString[c_stringSize];
		CStatsUtils::GetTime(nTimeInMs, szString, c_stringSize, true);

		wchar_t szStringTime[RL_MAX_DISPLAY_NAME_BUF_SIZE];
		String^ stringTime = rlXblCommon::UTF8ToWinRtString(szString, szStringTime, RL_MAX_DISPLAY_NAME_BUF_SIZE);

		ULONG result;
		if (sysAppContent::IsJapaneseBuild())
		{
			result = EventWriteJA_MP_PLAYING_TIME(playerId, &playerSessionId, stringTime->Data());
		}
		else
		{
			result = EventWriteMP_PLAYING_TIME(playerId, &playerSessionId, stringTime->Data());
		}

		gnetDebug1("EventWriteMP_PLAYING_TIME val=%" I64FMT "u : '%s', response: %lu", nTimeInMs, szString, result);

		return result == ERROR_SUCCESS;
	}
	rcatchall
	{

	}

	return false;
}

bool events_durango::WriteEvent_CashEarnedMP(s64 cashEarned)
{
	rtry
	{
		rcheck(cashEarned > 0, catchall, ); // don't write on stat clear

		int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
		rcheck(RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex), catchall, );

		PCWSTR playerId = GetUserId(localGamerIndex);

		GUID playerSessionId;
		ZeroMemory( &playerSessionId, sizeof(GUID) );

		char cashBuff[32];
		CFrontendStatsMgr::FormatInt64ToCash(cashEarned, cashBuff, sizeof(cashBuff)/sizeof(cashBuff[0]));

		wchar_t szStringCash[RL_MAX_DISPLAY_NAME_BUF_SIZE];
		String^ stringCash = rlXblCommon::UTF8ToWinRtString(cashBuff, szStringCash, RL_MAX_DISPLAY_NAME_BUF_SIZE);

		ULONG result;
		if (sysAppContent::IsJapaneseBuild())
		{
			result = EventWriteJA_MPPLY_TOTAL_EVC(playerId, &playerSessionId, stringCash->Data());
		}
		else
		{
			result = EventWriteMPPLY_TOTAL_EVC(playerId, &playerSessionId, stringCash->Data());
		}

		gnetDebug1("EventWriteMPPLY_TOTAL_EVC val=%d:'%s', response: %lu", cashEarned, cashBuff, result);
		return result == ERROR_SUCCESS;
	}
	rcatchall
	{

	}

	return false;
}

bool events_durango::WriteEvent_KillDeathRatio(float killDeathRatio)
{
	rtry
	{
		rcheck(killDeathRatio > 0, catchall, ); // don't write on stat clear

		int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
		rcheck(RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex), catchall, );

		PCWSTR playerId = GetUserId(localGamerIndex);

		GUID playerSessionId;
		ZeroMemory( &playerSessionId, sizeof(GUID) );

		killDeathRatio = round(killDeathRatio*100.0f)/100.0f;

		ULONG result;
		if (sysAppContent::IsJapaneseBuild())
		{
			result = EventWriteJA_MPPLY_KILL_DEATH_RATIO(playerId, &playerSessionId, killDeathRatio);
		}
		else
		{
			result = EventWriteMPPLY_KILL_DEATH_RATIO(playerId, &playerSessionId, killDeathRatio);
		}

		gnetDebug1("EventWriteMPPLY_KILL_DEATH_RATIO val=%f, response: %lu", killDeathRatio, result);
		return result == ERROR_SUCCESS;
	}
	rcatchall
	{

	}

	return false;
}

class XblWriteArchEnemyTask : public netXblTask
{
public:
	NET_TASK_USE_CHANNEL(rline_xbl);
	NET_TASK_DECL(XblWriteArchEnemyTask);

	XblWriteArchEnemyTask();
	bool Configure(const int localGamerIndex, const u64 archenemy);

	virtual void OnCancel();

protected:
	virtual bool DoWork();
	virtual void ProcessSuccess();

private:
	int m_LocalGamerIndex;
	rlGamerHandle m_ArchEnemy;
	rlGamertag m_GamerTag;
};

XblWriteArchEnemyTask::XblWriteArchEnemyTask()
	: m_LocalGamerIndex(-1)
{
	m_ArchEnemy.Clear();
	m_GamerTag[0] = '\0';
}

bool XblWriteArchEnemyTask::Configure(const int localGamerIndex, const u64 archenemy)
{
	rtry
	{
		rverify(RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex), catchall, );
		rverify(archenemy > 0, catchall, );

		m_LocalGamerIndex = localGamerIndex;
		m_ArchEnemy.ResetXbl(archenemy);

		return true;
	}
	rcatchall
	{
		return false;
	}
}

void XblWriteArchEnemyTask::OnCancel()
{
	netTaskDebug("Canceled while in state %d", m_State);
	if (m_MyStatus.Pending())
	{
		g_rlXbl.GetProfileManager()->CancelPlayerNameRequest(&m_MyStatus);
	}
}

bool XblWriteArchEnemyTask::DoWork()
{
	netTaskDebug3("Looking up archenemy name for XUID: %" I64FMT "u", m_ArchEnemy.GetXuid());

	// we're looking up the gamertag here instead of the display name since this name is going to be
	// displayed on a publicly viewable stat. We don't want someone's real name to be viewed by
	// people who don't have permissions to view that player's real name.
	return g_rlXbl.GetProfileManager()->GetPlayerNames(m_LocalGamerIndex, &m_ArchEnemy, 1, NULL, &m_GamerTag, &m_MyStatus);
}

void XblWriteArchEnemyTask::ProcessSuccess()
{
	rtry
	{
		char* archEnemyName = NULL;
		if (m_GamerTag[0] != '\0')
		{
			archEnemyName = m_GamerTag;
		}

		rcheck(archEnemyName, catchall, );

		int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
		rcheck(RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex), catchall, );

		PCWSTR playerId = events_durango::GetUserId(localGamerIndex);

		GUID playerSessionId;
		ZeroMemory( &playerSessionId, sizeof(GUID) );

		wchar_t archenemy[RL_MAX_DISPLAY_NAME_BUF_SIZE];
		String^ archEnemy = rlXblCommon::UTF8ToWinRtString(archEnemyName, archenemy, RL_MAX_DISPLAY_NAME_BUF_SIZE);

		OUTPUT_ONLY(ULONG result;);
		if (sysAppContent::IsJapaneseBuild())
		{
			OUTPUT_ONLY(result = )EventWriteJA_ARCHENEMY(playerId, &playerSessionId, archEnemy->Data());
		}
		else
		{
			OUTPUT_ONLY(result = )EventWriteARCHENEMY(playerId, &playerSessionId, archEnemy->Data());
		}

		gnetDebug1("EventWriteARCHENEMY val=%s, response: %lu", archEnemyName, result);
	}
	rcatchall
	{

	}
}


bool events_durango::WriteEvent_ArchEnemy(u64 enemy)
{
	netFireAndForgetTask<XblWriteArchEnemyTask>* task = NULL;

	rtry
	{
		rcheck(enemy > 0, catchall, );
		rcheck(enemy != m_LastArchenemy, catchall, );
		m_LastArchenemy = enemy;

		int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
		rcheck(RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex), catchall, );

		rcheck(netTask::Create(&task), catchall, );
		rcheck(task->Configure(localGamerIndex, enemy), catchall,);
		rcheck(netTask::Run(task), catchall, );

		return true;
	}
	rcatchall
	{
		netTask::Destroy(task);	
		return false;
	}
}

bool events_durango::WriteEvent_HighestRankMP(int highestRank)
{
	rtry
	{
		rcheck(highestRank > 0, catchall, ); // don't write on stat clear

		int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
		rcheck(RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex), catchall, );

		PCWSTR playerId = GetUserId(localGamerIndex);

		GUID playerSessionId;
		ZeroMemory( &playerSessionId, sizeof(GUID) );

		ULONG result;
		if (sysAppContent::IsJapaneseBuild())
		{
			result = EventWriteJA_HighestRankMP(playerId, &playerSessionId, highestRank);
		}
		else
		{
			result = EventWriteHighestRankMP(playerId, &playerSessionId, highestRank);
		}

		gnetDebug1("EventWriteHighestRankMP val=%d, response: %lu", highestRank, result);
		return result == ERROR_SUCCESS;
	}
	rcatchall
	{

	}

	return false;
}

void events_durango::OnStatWrite(StatId& statKey, void* pData, const unsigned sizeofData, const u32 )
{
	u32 keyHash = statKey.GetHash();

	rtry
	{
		if (STAT_RANK_FM == keyHash)
		{
			rverify(sizeof(int) == sizeofData, catchall,);
			events_durango::WriteEvent_HighestRankMP(*((int*)pData));
		}
		else if (STAT_TOTAL_CASH_EARNED.GetHash(CHAR_MICHEAL) == keyHash
			|| STAT_TOTAL_CASH_EARNED.GetHash(CHAR_FRANKLIN) == keyHash
			|| STAT_TOTAL_CASH_EARNED.GetHash(CHAR_TREVOR) == keyHash)
		{
			rverify(sizeof(int) == sizeofData, catchall,);

			const int cashEarned_Michael  = StatsInterface::GetIntStat(STAT_TOTAL_CASH_EARNED.GetHash(CHAR_MICHEAL));
			const int cashEarned_Franklin = StatsInterface::GetIntStat(STAT_TOTAL_CASH_EARNED.GetHash(CHAR_FRANKLIN));
			const int cashEarned_Trevor   = StatsInterface::GetIntStat(STAT_TOTAL_CASH_EARNED.GetHash(CHAR_TREVOR));

			const s64 totalcash = cashEarned_Michael + cashEarned_Franklin + cashEarned_Trevor;

#if !__NO_OUTPUT
			char cashBuff_Michael[32];
			CFrontendStatsMgr::FormatInt64ToCash(cashEarned_Michael, cashBuff_Michael, sizeof(cashBuff_Michael)/sizeof(cashBuff_Michael[0]));

			char cashBuff_Franklin[32];
			CFrontendStatsMgr::FormatInt64ToCash(cashEarned_Franklin, cashBuff_Franklin, sizeof(cashBuff_Franklin)/sizeof(cashBuff_Franklin[0]));

			char cashBuff_Trevor[32];
			CFrontendStatsMgr::FormatInt64ToCash(cashEarned_Trevor, cashBuff_Trevor, sizeof(cashBuff_Trevor)/sizeof(cashBuff_Trevor[0]));

			gnetDebug1("WriteEvent_CashEarnedSP MICHEAL='%d, %s', FRANKLIN='%d, %s', TREVOR='%d, %s'"
							,cashEarned_Michael,  cashBuff_Michael
							,cashEarned_Franklin, cashBuff_Franklin
							,cashEarned_Trevor,   cashBuff_Trevor);
#endif // !__NO_OUTPUT

			// Cash Earned in Single Player hero stat
			events_durango::WriteEvent_CashEarnedSP(totalcash);
		}
		else if (STAT_MONEY_TOTAL_SPENT.GetHash(CHAR_MICHEAL) == keyHash
			|| STAT_MONEY_TOTAL_SPENT.GetHash(CHAR_FRANKLIN) == keyHash
			|| STAT_MONEY_TOTAL_SPENT.GetHash(CHAR_TREVOR) == keyHash)
		{
			const s64 totalcash = StatsInterface::GetIntStat(STAT_MONEY_TOTAL_SPENT.GetHash(CHAR_MICHEAL)) 
									+ StatsInterface::GetIntStat(STAT_MONEY_TOTAL_SPENT.GetHash(CHAR_FRANKLIN))
									+ StatsInterface::GetIntStat(STAT_MONEY_TOTAL_SPENT.GetHash(CHAR_TREVOR));

			rverify(sizeof(int) == sizeofData, catchall,);

			if (STAT_MONEY_TOTAL_SPENT.GetHash(CHAR_TREVOR) == (u32)keyHash)
				events_durango::WriteEvent_MoneyTotalSpent((int)CHAR_TREVOR, totalcash);
			else  if (STAT_MONEY_TOTAL_SPENT.GetHash(CHAR_FRANKLIN) == (u32)keyHash) 
				events_durango::WriteEvent_MoneyTotalSpent((int)CHAR_FRANKLIN, totalcash);
			else
				events_durango::WriteEvent_MoneyTotalSpent((int)CHAR_MICHEAL, totalcash);
		}
		else if (STAT_MP_AWARD_HOLD_UP_SHOPS == keyHash)
		{
			rverify(sizeof(int) == sizeofData, catchall,);
			events_durango::WriteEvent_HeldUpShops(*((int*)pData));
		}
		else if (STAT_MP_AWARD_FMHORDWAVESSURVIVE == keyHash)
		{
			rverify(sizeof(int) == sizeofData, catchall,);
			events_durango::WriteEvent_HordeWavesSurvived(*((int*)pData));
		}
		else if (STAT_MPPLY_TOTAL_CUSTOM_RACES_WON == keyHash)
		{
			rverify(sizeof(int) == sizeofData, catchall,);
			events_durango::WriteEvent_TotalCustomRacesWon(*((int*)pData));
		}
		else if (STAT_MP_STAT_CHAR_FM_PLAT_AWARD_COUNT == keyHash)
		{
			rverify(sizeof(int) == sizeofData, catchall,);
			events_durango::WriteEvent_PlatinumAwards(*((int*)pData));
		}
		else if (STAT_KILLS.GetHash(CHAR_MICHEAL) == keyHash
			|| STAT_KILLS.GetHash(CHAR_FRANKLIN) == keyHash
			|| STAT_KILLS.GetHash(CHAR_TREVOR) == keyHash)
		{
			rverify(sizeof(int) == sizeofData, catchall,);

			const int kills = StatsInterface::GetIntStat(STAT_KILLS.GetHash(CHAR_MICHEAL)) 
								+ StatsInterface::GetIntStat(STAT_KILLS.GetHash(CHAR_FRANKLIN))
								+ StatsInterface::GetIntStat(STAT_KILLS.GetHash(CHAR_TREVOR));

			events_durango::WriteEvent_EnemyDefeatedSP(kills);
		}
		else if (STAT_DEATHS.GetHash(CHAR_MICHEAL) == keyHash
			|| STAT_DEATHS.GetHash(CHAR_FRANKLIN) == keyHash
			|| STAT_DEATHS.GetHash(CHAR_TREVOR) == keyHash)
		{
			rverify(sizeof(int) == sizeofData, catchall,);

			const int deaths = StatsInterface::GetIntStat(STAT_DEATHS.GetHash(CHAR_MICHEAL)) 
								+ StatsInterface::GetIntStat(STAT_DEATHS.GetHash(CHAR_FRANKLIN))
								+ StatsInterface::GetIntStat(STAT_DEATHS.GetHash(CHAR_TREVOR));

			events_durango::WriteEvent_PlayerDefeatedSP(deaths);
		}
		else if (STAT_MP_KILLS_PLAYERS == keyHash)
		{
			rverify(sizeof(int) == sizeofData, catchall,);
			events_durango::WriteEvent_PlayerKillsMP(*((int*)pData));
		}
		else if (STAT_NUMBER_STOLEN_CARS.GetHash(CHAR_MICHEAL) == keyHash
			|| STAT_NUMBER_STOLEN_CARS.GetHash(CHAR_FRANKLIN) == keyHash
			|| STAT_NUMBER_STOLEN_CARS.GetHash(CHAR_TREVOR) == keyHash)
		{
			rverify(sizeof(int) == sizeofData, catchall, );

			const int stolen = StatsInterface::GetIntStat(STAT_NUMBER_STOLEN_CARS.GetHash(CHAR_MICHEAL)) 
								+ StatsInterface::GetIntStat(STAT_NUMBER_STOLEN_CARS.GetHash(CHAR_FRANKLIN))
								+ StatsInterface::GetIntStat(STAT_NUMBER_STOLEN_CARS.GetHash(CHAR_TREVOR));

			events_durango::WriteEvent_StolenCars(stolen);
		}
		else if (STAT_STARS_ATTAINED.GetHash(CHAR_MICHEAL) == keyHash
			|| STAT_STARS_ATTAINED.GetHash(CHAR_FRANKLIN) == keyHash
			|| STAT_STARS_ATTAINED.GetHash(CHAR_TREVOR) == keyHash)
		{
			rverify(sizeof(int) == sizeofData, catchall, );

			const int stars_Michael  = StatsInterface::GetIntStat(STAT_STARS_ATTAINED.GetHash(CHAR_MICHEAL));
			const int stars_Franklin = StatsInterface::GetIntStat(STAT_STARS_ATTAINED.GetHash(CHAR_FRANKLIN));
			const int stars_Trevor   = StatsInterface::GetIntStat(STAT_STARS_ATTAINED.GetHash(CHAR_TREVOR));

			int stars = (*((int*)pData));
			stars = stars > stars_Michael  ? stars : stars_Michael;
			stars = stars > stars_Franklin ? stars : stars_Franklin;
			stars = stars > stars_Trevor   ? stars : stars_Trevor;

			events_durango::WriteEvent_StarsAttained((unsigned)stars);
		}
		else if (STAT_MPPLY_TOTAL_EVC == keyHash)
		{
			rverify(sizeof(int64) == sizeofData, catchall, );
			events_durango::WriteEvent_CashEarnedMP(StatsInterface::GetInt64Stat(STAT_MPPLY_TOTAL_EVC));
		}
		else if (STAT_MPPLY_KILL_DEATH_RATIO == keyHash)
		{
			rverify(sizeof(float) == sizeofData, catchall, );
			events_durango::WriteEvent_KillDeathRatio(*((float*)pData));
		}
		else if (STAT_ARCHENEMY == keyHash)
		{
			u64 xuid = 0;

			if (sizeof(u64) == sizeofData)
			{
				xuid = *((u64*)pData);
			}
			else
			{
				char* text = (char*)pData;
				rverify(sscanf_s(text, "%" I64FMT "u", &xuid) == 1, catchall, );
			}
			
			rcheck(xuid > 0, catchall, gnetDebug3("No archenemy to write"));
			gnetDebug3("Updating archenemy: %" I64FMT "u", xuid);
			events_durango::WriteEvent_ArchEnemy(xuid);
		}
	}
	rcatchall
	{

	}
}

void events_durango::OnPreSavegame(StatId& statKey, void* pData, const unsigned sizeofData )
{
	u32 keyHash = statKey.GetHash();

	rtry
	{
		if (STAT_TOTAL_PROGRESS_MADE == keyHash)
		{
			rverify(sizeof(float) == sizeofData, catchall,);
			events_durango::WriteEvent_GameProgressSP(*((float*)pData));
		}
		else if (STAT_PLAYING_TIME == keyHash)
		{
			rverify(sizeof(u64) == sizeofData, catchall, );
			events_durango::WriteEvent_PlayingTime(*((u64*)pData));
		}
		else if (STAT_MP_PLAYING_TIME == keyHash)
		{
			rverify(sizeof(u64) == sizeofData, catchall, );
			events_durango::WriteEvent_MultiplayerPlaytime(*((u64*)pData));
		}
	}
	rcatchall
	{

	}
}

void events_durango::ClearAchievementProgress()
{
	for (int i = 0; i < XB1_ACHIEVEMENT_EVENTS; i++)
		sm_iPreviousAchievementProgress[i] = -1;

	gnetDebug3("events_durango::ClearAchievementProgress - Resetting cached achievement progress");
}

int events_durango::GetAchievementProgress(int achId)
{
	rtry
	{
		rverify(achId >= 0 && achId < XB1_ACHIEVEMENT_EVENTS, catchall, );
		return sm_iPreviousAchievementProgress[achId];
	}
	rcatchall
	{
		return 0;
	}
}

#endif  //RSG_DURANGO

