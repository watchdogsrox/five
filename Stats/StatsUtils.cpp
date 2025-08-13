//
// StatsUtils.cpp
//
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved.
//


// --- Include Files ------------------------------------------------------------

// C headers

// Rage headers

// Stats headers
#include "StatsUtils.h"
#include "Stats/StatsInterface.h"
#include "Stats/StatsDataMgr.h"
#include "Stats/StatsMgr.h"
#include "Stats/stats_channel.h"

// Network headers
#include "Network/NetworkInterface.h"

// Game headers
#include "Frontend/PauseMenu.h"
#include "Text/Text.h"
#include "Text/TextConversion.h"
#include "Peds/ped.h"
#include "Modelinfo/PedModelInfo.h"
#include "control/replay/replay.h"

FRONTEND_STATS_OPTIMISATIONS()
SAVEGAME_OPTIMISATIONS()


#define RETURNED_STRING_SIZE (128)
#define STRING_SEGMENT_SIZE (32)

//Used to disable the tracking of the stats
bool CStatsUtils::sm_enabled=true;

/*
void 
CStatsUtils::CreateFrontendStatLine(char* buffer, s32 listId, s32 statPosInList)
{
	sDisplayListTypes* pList = StatsInterface::GetDisplayListP(listId);
	if (AssertVerify(pList && statPosInList>=0 && statPosInList < StatsInterface::GetListMemberCount(listId)))
	{
		const StatId& keyHash = pList->m_aStats[statPosInList].GetId();

		char text[256];

		if (STAT_FASTEST_SPEED == keyHash || STAT_TOP_SPEED_CAR == keyHash)
		{
			CStatsUtils::BuildStatLine(pList->m_aStats[statPosInList].GetHeader(), 
										(char*)&text[0], 
										(char*)buffer, 
										STAT_TOP_SPEED_CAR, 
										STAT_TYPE_STRING, 
										STAT_FASTEST_SPEED, 
										STAT_TYPE_MILES, 
										2);
		}
		else
		{
			CStatsUtils::BuildStatLine(pList->m_aStats[statPosInList].GetHeader(), 
										(char*)&text[0], 
										(char*)buffer, 
										keyHash, 
										pList->m_aStats[statPosInList].GetType());
		}
	}
}
*/



const char* 
	CStatsUtils::GetTime(u64 iTimeInMilliseconds, char* outBuffer, u32 bufferSize, bool bStatsScreenFormatting)
{
	static char bufferRet[RETURNED_STRING_SIZE];
	bufferRet[0] = '\0';

	if (statVerifyf(bufferSize <= RETURNED_STRING_SIZE, "Pass string size must be <%d", RETURNED_STRING_SIZE))
	{
		if (iTimeInMilliseconds == 0)
		{
			::sprintf(outBuffer, "%s",bStatsScreenFormatting ? "0d 0h 0m 0s" : "0:00:00:00");
			return bStatsScreenFormatting ? "0d 0h 0m 0s" : "0:00:00:00";
		}

		//char bufferSec[20];
		u64 iSec = (u64)rage::Floorf((float)iTimeInMilliseconds/1000);
		iTimeInMilliseconds = iTimeInMilliseconds % 1000;
		//::sprintf(&bufferSec[0], "%s%s%d", ((iTimeInMilliseconds>99)?"":"0"), ((iTimeInMilliseconds>9)?"":"0"), iTimeInMilliseconds);

		char bufferSec[STRING_SEGMENT_SIZE];
		bufferSec[0] = '\0';

		u64 iMin = (u64)rage::Floorf((float)iSec/60);
		iSec = iSec % 60;
		if(bStatsScreenFormatting)
		{
			::sprintf(&bufferSec[0], "%" I64FMT "ds", iSec);
		}
		else
		{
			::sprintf(&bufferSec[0], "%s%" I64FMT "d", ((iSec>9)?"":"0"), iSec);
		}

		char bufferMin[STRING_SEGMENT_SIZE * 2];  // stores bufferSec + its own text
		bufferMin[0] = '\0';

		u64 iHours = (u64)rage::Floorf((float)iMin/60);
		iMin = iMin % 60;
		if(bStatsScreenFormatting)
		{
			::sprintf(&bufferMin[0], "%" I64FMT "dm %s", iMin, &bufferSec[0]);
		}
		else
		{
			::sprintf(&bufferMin[0], "%s%" I64FMT "d:%s", ((iMin>9)?"":"0"), iMin, &bufferSec[0]);
		}

		char bufferHour[STRING_SEGMENT_SIZE * 2];  // stores bufferMin + its own text
		bufferHour[0] = '\0';
		u64 iDays = (u64)rage::Floorf((float)iHours/24);

		iHours = iHours % 24;
		if(bStatsScreenFormatting)
		{
			::sprintf(&bufferHour[0], "%" I64FMT "dh %s", iHours, &bufferMin[0]);
		}
		else
		{
			::sprintf(&bufferHour[0], "%s%" I64FMT "d:%s", ((iHours>9)?"":"0"), iHours, &bufferMin[0]);
		}

		if(bStatsScreenFormatting)
		{
			::sprintf(&bufferRet[0], "%" I64FMT "dd %s", iDays, &bufferHour[0]);
		}
		else
		{
			::sprintf(&bufferRet[0], "%" I64FMT "d:%s", iDays, &bufferHour[0]);
		}

		u64 iLength = istrlen(&bufferRet[0]);
		bufferRet[iLength-1] = '\0';

		::sprintf(outBuffer, "%s",bufferRet);
	}
	else
	{
		bufferRet[0] = '\0';
	}
	return &bufferRet[0];
}



const char* 
	CStatsUtils::GetTime(s32 iTimeInMilliseconds, char* outBuffer, u32 bufferSize, bool bStatsScreenFormatting)
{
	if (statVerifyf(bufferSize <= RETURNED_STRING_SIZE, "Pass string size must be < %d", RETURNED_STRING_SIZE))
	{
		if (iTimeInMilliseconds == 0)
		{
			::sprintf(outBuffer, "%s",bStatsScreenFormatting ? "0d 0h 0m 0s" : "0:00:00:00");
			return bStatsScreenFormatting ? "0d 0h 0m 0s" : "0:00:00:00";
		}

		//char bufferSec[20];
		s32 iSec = (s32)rage::Floorf((float)iTimeInMilliseconds/1000);
		iTimeInMilliseconds = iTimeInMilliseconds % 1000;
		//::sprintf(&bufferSec[0], "%s%s%d", ((iTimeInMilliseconds>99)?"":"0"), ((iTimeInMilliseconds>9)?"":"0"), iTimeInMilliseconds);

		char bufferSec[STRING_SEGMENT_SIZE];
		bufferSec[0] = '\0';

		s32 iMin = (s32)rage::Floorf((float)iSec/60);
		iSec = iSec % 60;
		if(bStatsScreenFormatting)
		{
			::sprintf(&bufferSec[0], "%ds", iSec);
		}
		else
		{
			::sprintf(&bufferSec[0], "%s%d", ((iSec>9)?"":"0"), iSec);
		}

		char bufferMin[STRING_SEGMENT_SIZE];
		bufferMin[0] = '\0';

		s32 iHours = (s32)rage::Floorf((float)iMin/60);
		iMin = iMin % 60;
		if(bStatsScreenFormatting)
		{
			::sprintf(&bufferMin[0], "%dm %s", iMin, &bufferSec[0]);
		}
		else
		{
			::sprintf(&bufferMin[0], "%s%d:%s", ((iMin>9)?"":"0"), iMin, &bufferSec[0]);
		}

		char bufferHour[STRING_SEGMENT_SIZE * 2]; // as it has its own text + bufferMin
		bufferHour[0] = '\0';

		s32 iDays = (s32)rage::Floorf((float)iHours/24);
		iHours = iHours % 24;
		if(bStatsScreenFormatting)
		{
			::sprintf(&bufferHour[0], "%dh %s", iHours, &bufferMin[0]);
		}
		else
		{
			::sprintf(&bufferHour[0], "%s%d:%s", ((iHours>9)?"":"0"), iHours, &bufferMin[0]);
		}

		char bufferRet[RETURNED_STRING_SIZE];
		bufferRet[0] = '\0';

		if(bStatsScreenFormatting)
		{
			::sprintf(&bufferRet[0], "%dd %s", iDays, &bufferHour[0]);
		}
		else
		{
			::sprintf(&bufferRet[0], "%d:%s", iDays, &bufferHour[0]);
		}

		s32 iLength = istrlen(&bufferRet[0]);
		bufferRet[iLength-1] = '\0';


		::sprintf(outBuffer, "%s",bufferRet);
	}
	else
	{
		outBuffer[0] = '\0';
	}

	return outBuffer;
}



s32
CStatsUtils::ConvertToDays(u64 iTimeInMilliseconds)
{
	s32 iSec   = (s32)(iTimeInMilliseconds/1000);
	s32 iMin   = iSec/60;
	s32 iHours = iMin/60;
	s32 iDays  = iHours/24;

	return iDays;
}

s32
CStatsUtils::ConvertToHours(u64 iTimeInMilliseconds)
{
	s32 iSec   = (s32)(iTimeInMilliseconds/1000);
	s32 iMin   = iSec/60;
	s32 iHours = iMin/60;
	iHours = iHours % 24;

	return iHours;
}

s32
CStatsUtils::ConvertToMins(u64 iTimeInMilliseconds)
{
	s32 iSec = (s32)(iTimeInMilliseconds/1000);
	s32 iMin = iSec/60;
	iMin = iMin % 60;

	return iMin;
}

s32 
CStatsUtils::ConvertToSeconds(u64 iTimeInMilliseconds)
{
	s32 iSec = (s32)(iTimeInMilliseconds/1000);
	iSec = iSec % 60;

	return iSec;
}

s32 
CStatsUtils::ConvertToMilliseconds(u64 iTimeInMilliseconds)
{
	return ((s32)iTimeInMilliseconds % 1000);
}


// --- Private Methods --------------------------------------------------
void 
CStatsUtils::BuildStatLine(const char* pLabel, 
						   char* pStatLabel, 
						   char* pStatValueString, 
						   const StatId keyHash1,
						   const int statType1,
						   const StatId keyHash2,
						   const int statType2,
						   const int type)
{
	CPed* playerPed = FindPlayerPed();
	CPedModelInfo* mi = playerPed ? playerPed->GetPedModelInfo() : NULL;
	if (!mi)
		return;

	int inum1, inum2;
	float fnum1, fnum2;

	// invalid input?
	if( !pLabel || !pStatLabel || !pStatValueString )
	{
		Assert(0);
		return;
	}

	pStatValueString[0] = 0;

	switch (type)
	{
	case STAT_LINE_TYPE_1:
		{
			Assert(StatsInterface::GetIsBaseType(keyHash2, STAT_TYPE_INT) || StatsInterface::GetIsBaseType(keyHash2, STAT_TYPE_FLOAT));

			int n1 = 1;
			//if (STAT_VEHICLES_BLOWN_UP == keyHash1)
			//{
			//	n1 = StatsInterface::GetIntStat(mi->GetStatsHashId("CARS_EXPLODED"))  + 
			//		 StatsInterface::GetIntStat(mi->GetStatsHashId("BIKES_EXPLODED")) + 
			//		 StatsInterface::GetIntStat(mi->GetStatsHashId("BOATS_EXPLODED")) + 
			//		 StatsInterface::GetIntStat(mi->GetStatsHashId("HELIS_EXPLODED")) + 
			//		 StatsInterface::GetIntStat(mi->GetStatsHashId("TRAILER_EXPLODED")) + 
			//		 StatsInterface::GetIntStat(mi->GetStatsHashId("QUADBIKE_EXPLODED")) + 
			//		 StatsInterface::GetIntStat(mi->GetStatsHashId("AUTOGYRO_EXPLODED")) + 
			//		 StatsInterface::GetIntStat(mi->GetStatsHashId("BICYCLE_EXPLODED")) + 
			//		 StatsInterface::GetIntStat(mi->GetStatsHashId("SUBMARINE_EXPLODED")) + 
			//		 StatsInterface::GetIntStat(mi->GetStatsHashId("TRAIN_EXPLODED")) + 
			//		 StatsInterface::GetIntStat(mi->GetStatsHashId("PLANES_EXPLODED"));
			//}

			int n2 = 0;
			if (StatsInterface::GetIsBaseType(keyHash2, STAT_TYPE_FLOAT))
			{
				n2 = (int)StatsInterface::GetFloatStat(keyHash2);
			}
			else if (StatsInterface::GetIsBaseType(keyHash2, STAT_TYPE_INT))
			{
				n2 = StatsInterface::GetIntStat(keyHash2);
			}

			if (n2 < 10)
			{
				::sprintf(pStatValueString, "%d:0%d", n1, n2);
			}
			else
			{
				::sprintf(pStatValueString, "%d:%d", n1, n2);
			}
		}
		break;

	case STAT_LINE_TYPE_2:
		{
			char statString1[256];
			char statString2[256];
			statString1[0] = 0;
			statString2[0] = 0;
			CreateOneStatLine(&statString1[0], keyHash1, statType1);
			CreateOneStatLine(&statString2[0], keyHash2, statType2);
			::sprintf(pStatValueString, "%s %s", &statString1[0], &statString2[0]);
		}
		break;

	default:
		// is second number available?
		if(keyHash2.IsValid())
		{
			switch(statType1)
			{
			case STAT_TYPE_INT:
				{
					Assert(StatsInterface::GetIsBaseType(keyHash1, STAT_TYPE_INT) && StatsInterface::GetIsBaseType(keyHash2, STAT_TYPE_INT));

					inum1 = StatsInterface::GetIntStat(keyHash1);
					inum2 = StatsInterface::GetIntStat(keyHash2);
					::sprintf(pStatValueString, " %d %s %d", inum1, TheText.Get("FEST_OO"), inum2);
				}
				break;
			case STAT_TYPE_FLOAT:
				{
					Assert(StatsInterface::GetIsBaseType(keyHash1, STAT_TYPE_FLOAT) && StatsInterface::GetIsBaseType(keyHash2, STAT_TYPE_FLOAT));

					fnum1 = StatsInterface::GetFloatStat(keyHash1);
					fnum2 = StatsInterface::GetFloatStat(keyHash2);
					::sprintf(pStatValueString, "%.2f %s %.2f", fnum1, TheText.Get("FEST_OO"), fnum2);

				}
				break;
			case STAT_TYPE_CASH:
				{
					Assert(StatsInterface::GetIsBaseType(keyHash1, STAT_TYPE_INT) || StatsInterface::GetIsBaseType(keyHash1, STAT_TYPE_FLOAT));
					Assert(StatsInterface::GetIsBaseType(keyHash2, STAT_TYPE_INT) || StatsInterface::GetIsBaseType(keyHash2, STAT_TYPE_FLOAT));

					fnum1 = 0.0f;
					fnum2 = 0.0f;

					if (StatsInterface::GetIsBaseType(keyHash1, STAT_TYPE_FLOAT))
					{
						fnum1 = StatsInterface::GetFloatStat(keyHash1);
					}
					else if (StatsInterface::GetIsBaseType(keyHash1, STAT_TYPE_INT))
					{
						fnum1 = (float)StatsInterface::GetIntStat(keyHash1);
					}

					if (StatsInterface::GetIsBaseType(keyHash2, STAT_TYPE_FLOAT))
					{
						fnum2 = StatsInterface::GetFloatStat(keyHash2);
					}
					else if (StatsInterface::GetIsBaseType(keyHash2, STAT_TYPE_INT))
					{
						fnum2 = (float)StatsInterface::GetIntStat(keyHash2);
					}

					if (CPauseMenu::GetMenuPreference(PREF_CURRENT_LANGUAGE) != LANGUAGE_SPANISH && CPauseMenu::GetMenuPreference(PREF_CURRENT_LANGUAGE) != LANGUAGE_FRENCH)
					{
						::sprintf(pStatValueString, "$%d %s $%d", (int)ABS(fnum1), TheText.Get("FEST_OO"), (int)ABS(fnum2));
					}
					else
					{
						::sprintf(pStatValueString, "%d$ %s %d$", (int)ABS(fnum1), TheText.Get("FEST_OO"), (int)ABS(fnum2));
					}
				}
				break;

			default:
				break;
			}
		}
		else if (AssertVerify(keyHash1.IsValid()))
		{
			CreateOneStatLine(pStatValueString, keyHash1, statType1);
		}
		break;
	}

	strcpy( pStatLabel, TheText.Get(pLabel) );
	CText::FilterOutTokensFromString(pStatLabel); // remove any markers
}

void 
CStatsUtils::CreateOneStatLine(char* statString, const StatId& keyHash, const int statType)
{
	CPed* playerPed = FindPlayerPed();
	statAssert(playerPed);
	if (!playerPed)
		return;

	CPedModelInfo* mi =	playerPed->GetPedModelInfo();
	statAssert(mi);
	if (!mi)
		return;

	statString[0] = '\0';

	if (STAT_TOTAL_PROGRESS_MADE == keyHash)
	{
		float fPercentage = CStatsMgr::GetPercentageProgress();
		::sprintf(statString, "%0.2f%%", fPercentage);
	}
	else if (STAT_KILLS_IN_FREE_AIM == keyHash)
	{
		float fPercentage = CStatsMgr::GetPercentageKillsMadeinFreeAim();
		::sprintf(statString, "%0.2f%%", fPercentage);
	}

	/*
	else if (STAT_SHOOTING_ACCURACY == keyHash)
	{
		float fPercentage = 0.0f;

		int iFired = StatsInterface::GetIntStat(mi->GetStatsHashId("SHOTS"));
		int iHit  = StatsInterface::GetIntStat(mi->GetStatsHashId("HITS"));

		fPercentage = (iFired==0.0f || iHit==0.0f) ? 0.0f : rage::Floorf((iHit*100.0f)/iFired);

		::sprintf(statString, "%0.2f%%", fPercentage);
	}
	else if (STAT_MONEY_MADE_FROM_RANDOM_PEDS == keyHash)
	{
		float fnum1 = StatsInterface::GetFloatStat(STAT_MONEY_MADE_FROM_USJS);
		float fnum2 = StatsInterface::GetFloatStat(STAT_MONEY_PICKED_UP_ON_STREET);
		float fnum3 = StatsInterface::GetFloatStat(STAT_MONEY_MADE_FROM_CAR_THEFTS);
		float fnum4 = StatsInterface::GetFloatStat(STAT_MONEY_MADE_IN_STREET_RACES);
		float fnum5 = StatsInterface::GetFloatStat(STAT_MONEY_MADE_FROM_MISSIONS);
		float fnum6 = StatsInterface::GetFloatStat(STAT_MONEY_MADE_FROM_VIGILANTE);

		float fnum7 = StatsInterface::GetFloatStat(STAT_CURRENT_MONEY);

		float fSpent = StatsInterface::GetFloatStat(STAT_MONEY_SPENT_ON_PAY_N_SPRAY)    +
						StatsInterface::GetFloatStat(STAT_MONEY_SPENT_ON_DATES)         +
						StatsInterface::GetFloatStat(STAT_MONEY_SPENT_BUYING_CLOTHES)   +
						StatsInterface::GetFloatStat(STAT_MONEY_SPENT_IN_BARS_CLUBS)    +
						StatsInterface::GetFloatStat(STAT_MONEY_SPENT_ON_PROSTITUTES)   +
						StatsInterface::GetFloatStat(STAT_MONEY_SPENT_IN_STRIP_CLUBS)   +
						StatsInterface::GetFloatStat(STAT_MONEY_SPENT_ON_FOOD)          +
						StatsInterface::GetFloatStat(STAT_MONEY_SPENT_ON_TAXIS)         +
						StatsInterface::GetFloatStat(STAT_MONEY_LOST_ON_STREET_RACES)   +
						StatsInterface::GetFloatStat(STAT_MONEY_SPENT_ON_COP_BRIDES)    +
						StatsInterface::GetFloatStat(STAT_MONEY_SPENT_ON_HEALTHCARE)    +
						StatsInterface::GetFloatStat(STAT_MONEY_GIVEN_TO_TRAMPS)        +
						StatsInterface::GetFloatStat(STAT_MONEY_SPENT_ON_VENDORS)       +
						StatsInterface::GetFloatStat(STAT_MONEY_SPENT_ON_TELESCOPES)    +
						StatsInterface::GetFloatStat(STAT_MONEY_SPENT_ON_BUYING_GUNS);

		float fTotal = fnum7+fSpent - (fnum1+fnum2+fnum3+fnum4+fnum5+fnum6);
		Assertf(fTotal>=0, "CStatsUtils::CreateOneStatLine() - Stat STAT_MONEY_MADE_FROM_RANDOM_PEDS is inferior to zero ()", fTotal);

		if (CPauseMenu::GetMenuPreference(PREF_CURRENT_LANGUAGE) != LANGUAGE_SPANISH && CPauseMenu::GetMenuPreference(PREF_CURRENT_LANGUAGE) != LANGUAGE_FRENCH)
		{
			::sprintf(statString, "$%d", (int)ABS(fTotal));
		}
		else
		{
			::sprintf(statString, "%d$", (int)ABS(fTotal));
		}
	}
	*/
	else
	{
		switch (statType)
		{
		case STAT_TYPE_INT:
			{ // number is s32:
				s32 inum1;
				inum1 = (s32)StatsInterface::GetIntStat(keyHash);
				::sprintf(statString, "%d", inum1);
			}
			break;
		case STAT_TYPE_FLOAT:
			{ // number is float:
				float fnum1;
				fnum1 = StatsInterface::GetFloatStat(keyHash);
				::sprintf(statString, "%.2f", fnum1);
			}
			break;
		case STAT_TYPE_PERCENT:
			{ // number is float:
				float fnum1;
				fnum1 = StatsInterface::GetFloatStat(keyHash);
				::sprintf(statString, "%0.2f%%", fnum1);
			}
			break;
		case STAT_TYPE_DEGREES:
			{ // number is s32:
				s32 inum1;
				inum1 = (s32)StatsInterface::GetIntStat(keyHash);
				::sprintf(statString, "%d|", inum1);
			}
			break;
		case STAT_TYPE_WEIGHT:
			{ // number is float:
				s32 inum1;
				inum1 = (s32)StatsInterface::GetFloatStat(keyHash);

				s32 iLanguage = CPauseMenu::GetMenuPreference(PREF_CURRENT_LANGUAGE);
				bool bMetric = (LANGUAGE_ENGLISH!=iLanguage);
				if (bMetric)
					::sprintf(statString, "%dkgs",(s32) LBS_TO_KGS(inum1));
				else
					::sprintf(statString, "%dlbs",inum1);
			}
			break;
		case STAT_TYPE_CASH:
			{ // number is cash:
				float fnum1 = 0.0f;
				if (StatsInterface::GetIsBaseType(keyHash, STAT_TYPE_FLOAT))
				{
					fnum1 = StatsInterface::GetFloatStat(keyHash);
				}
				else if (StatsInterface::GetIsBaseType(keyHash, STAT_TYPE_INT))
				{
					fnum1 = (float)StatsInterface::GetIntStat(keyHash);
				}

				if (CPauseMenu::GetMenuPreference(PREF_CURRENT_LANGUAGE) != LANGUAGE_SPANISH && CPauseMenu::GetMenuPreference(PREF_CURRENT_LANGUAGE) != LANGUAGE_FRENCH)
				{
					::sprintf(statString, "$%d", (int)ABS(fnum1)); // "$%.2f"
				}
				else
				{
					::sprintf(statString, "%d$", (int)ABS(fnum1)); // "$%.2f"
				}
			}
			break;
		case STAT_TYPE_METERS:
			{ // number is metres:
				float fnum1 = 0.0f;
				if (StatsInterface::GetIsBaseType(keyHash, STAT_TYPE_FLOAT))
				{
					fnum1 = StatsInterface::GetFloatStat(keyHash);
				}
				else if (StatsInterface::GetIsBaseType(keyHash, STAT_TYPE_INT))
				{
					fnum1 = (float)StatsInterface::GetIntStat(keyHash);
				}
				::sprintf(statString, "%.2fm", fnum1);
			}
			break;
		case STAT_TYPE_TIME:
			{
				const u32 c_szTimeSize = 128;
				char szTime[c_szTimeSize];
				GetTime(StatsInterface::GetUInt64Stat(keyHash), szTime, c_szTimeSize);

				::sprintf(statString, "%s", szTime);
			}
			break;
		case STAT_TYPE_DATE:
			int year;
			int month;
			int day;
			int hour;
			int minutes;
			int seconds;
			int milliseconds;

			StatsInterface::UnPackDate(StatsInterface::GetUInt64Stat(keyHash), year, month, day, hour, minutes,seconds, milliseconds);
			::sprintf(statString, "%d//%d//%d",year,month,day);

			break;
		case STAT_TYPE_FEET:
			{ // number is feet
				float fnum1 = 0.0f;
				if (StatsInterface::GetIsBaseType(keyHash, STAT_TYPE_FLOAT))
				{
					fnum1 = StatsInterface::GetFloatStat(keyHash);
				}
				else if (StatsInterface::GetIsBaseType(keyHash, STAT_TYPE_INT))
				{
					fnum1 = (float)StatsInterface::GetIntStat(keyHash);
				}
				::sprintf(statString, "%.2fft", METERS_TO_FEET(fnum1));
			}
			break;
		case STAT_TYPE_MILES:
			{ // number is float:
				::sprintf(statString, "%.2f", CStatsUtils::GetLanguageMetricValue(keyHash));
			}
			break;
		case STAT_TYPE_VELOCITY:
			{ // number is float:
				::sprintf(statString, "%.2f %s", CStatsUtils::GetLanguageMetricValue(keyHash), TheText.Get("ST_VELO") );
			}
			break;
		case STAT_TYPE_SECONDS:
			{ // number is float:
				u64 iVal = 0;
				if (StatsInterface::GetIsBaseType(keyHash, STAT_TYPE_UINT64))
				{
					iVal = StatsInterface::GetUInt64Stat(keyHash);
				}
				else if (StatsInterface::GetIsBaseType(keyHash, STAT_TYPE_FLOAT))
				{
					iVal = (u64)StatsInterface::GetFloatStat(keyHash);
				}
				else if (StatsInterface::GetIsBaseType(keyHash, STAT_TYPE_INT))
				{
					iVal = (u64)StatsInterface::GetIntStat(keyHash);
				}

				s32 days  = CStatsUtils::ConvertToDays(iVal);
				s32 hours = CStatsUtils::ConvertToHours(iVal) + days*24;
				s32 mins  = CStatsUtils::ConvertToMins(iVal);
				s32 secs  = CStatsUtils::ConvertToSeconds(iVal);
				::sprintf(statString, "%02d:%02d:%02d", hours, mins, secs);
			}
			break;
		case STAT_TYPE_TEXTLABEL:
			{ // number is string:
				char* pStatText = TheText.Get(StatsInterface::GetIntStat(keyHash), "STAT_TYPE_TEXTLABEL");
				if (pStatText)
				{
					CText::FilterOutTokensFromString(pStatText); // remove any markers
					::sprintf(statString, "%s", pStatText);
				}
				else
				{
					//::sprintf(statString, "%s", keyHash.GetName());
				}
				break;
			}

		default:
			{
				::sprintf(statString, "NOTYPE:%d", StatsInterface::GetStatType(keyHash));
			}
			break;
		}
	}
}

bool 
CStatsUtils::CheckForThreshold(float *stored_value, float new_value)
{
	bool valid_new_value = FALSE;

	if (new_value > *stored_value+STAT_DISPLAY_THRESHOLD) valid_new_value = TRUE;
	else
		if (new_value < *stored_value-STAT_DISPLAY_THRESHOLD) valid_new_value = TRUE;

	if (valid_new_value)
	{
		*stored_value = new_value;
	}

	return (valid_new_value);
}


bool  
CStatsUtils::CheckStatsForCombination(const StatId& keyHash)
{
	if (STAT_FASTEST_SPEED == keyHash)
	{
		return (NULL != TheText.Get(StatsInterface::GetIntStat(STAT_TOP_SPEED_CAR.GetStatId()), "STAT_TOP_SPEED_CAR"));
	}

	return true;
}

bool 
CStatsUtils::SafeToShowThisStatInTheCurrentLanguage(const StatId& keyHash)
{
	StatId statHeadshots  = StatsInterface::GetStatsModelHashId("HEADSHOTS");
	StatId statLegitKills = StatsInterface::GetStatsModelHashId("TOTAL_LEGITIMATE_KILLS");

#if defined(__GERMAN_BUILD) && __GERMAN_BUILD

	if(statLegitKills == keyHash || statHeadshots == keyHash)
	{
		return false;
	}

#else

	const int iLanguage = CPauseMenu::GetMenuPreference(PREF_CURRENT_LANGUAGE);

	// some stats are not in the german version:
	if(LANGUAGE_GERMAN==iLanguage && (statLegitKills == keyHash || statHeadshots == keyHash ))
	{
		return false;
	}

#endif // __GERMAN_BUILD

	return true;
}

bool 
CStatsUtils::IsStatCapped(const StatId& /*keyHash*/)
{
	return false;
}

bool 
CStatsUtils::IsStatsTrackingEnabled() 
{
	return sm_enabled REPLAY_ONLY(&& !CReplayMgr::IsReplayInControlOfWorld()); 
}
void 
CStatsUtils::EnableStatsTracking(bool val/*=true*/) 
{
	sm_enabled = val; 
}
void 
CStatsUtils::DisableStatsTracking()
{
	statDebugf3("Disable stats tracking !");
	sm_enabled = false; 
}

void 
CStatsUtils::ResetStatsTracking()
{
	statDebugf3("Reseting stat tracking to default (true)");
	sm_enabled=true;
}

bool 
CStatsUtils::CanUpdateStats( )
{
	return IsStatsTrackingEnabled();
}


float 
CStatsUtils::GetLanguageMetricValue(const StatId& keyHash)
{
	if (statVerify(StatsInterface::IsKeyValid(keyHash)))
	{
		s32 iLanguage = CPauseMenu::GetMenuPreference(PREF_CURRENT_LANGUAGE);
		switch(iLanguage)
		{
		case LANGUAGE_FRENCH:
			return CStatsUtils::GetFrenchLanguageMetricValue(keyHash);
		case LANGUAGE_GERMAN:
			return CStatsUtils::GetGermanLanguageMetricValue(keyHash);
		case LANGUAGE_ITALIAN:
			return CStatsUtils::GetItalianLanguageMetricValue(keyHash);
		case LANGUAGE_SPANISH:
			return CStatsUtils::GetSpanishLanguageMetricValue(keyHash);
		case LANGUAGE_JAPANESE:
			return CStatsUtils::GetJapaneseLanguageMetricValue(keyHash);
		case LANGUAGE_ENGLISH:
		default:
			return CStatsUtils::GetDefaultLanguageMetricValue(keyHash);
		}
	}

	return 0.0f;
}

float 
CStatsUtils::GetFrenchLanguageMetricValue(const StatId& keyHash)
{
	statAssert(StatsInterface::IsKeyValid(keyHash));

	float fnum1;
	fnum1 = StatsInterface::GetFloatStat(keyHash);

	if (STAT_AVERAGE_SPEED == keyHash)
	{
		return fnum1/1000;
	}

	return fnum1/1000;
}

float 
CStatsUtils::GetItalianLanguageMetricValue(const StatId& keyHash)
{
	statAssert(StatsInterface::IsKeyValid(keyHash));

	float fnum1;
	fnum1 = StatsInterface::GetFloatStat(keyHash);

	if (STAT_AVERAGE_SPEED == keyHash)
	{
		return fnum1/1000;
	}

	return fnum1/1000;
}
float 
CStatsUtils::GetSpanishLanguageMetricValue(const StatId& keyHash)
{
	statAssert(StatsInterface::IsKeyValid(keyHash));

	float fnum1;
	fnum1 = StatsInterface::GetFloatStat(keyHash);

	if (STAT_AVERAGE_SPEED == keyHash)
	{
		return fnum1/1000;
	}

	return fnum1/1000;
}
float 
CStatsUtils::GetJapaneseLanguageMetricValue(const StatId& keyHash)
{
	statAssert(StatsInterface::IsKeyValid(keyHash));

	float fnum1;
	fnum1 = StatsInterface::GetFloatStat(keyHash);

	if (STAT_AVERAGE_SPEED == keyHash)
	{
		return fnum1/1000;
	}

	return fnum1/1000;
}
float 
CStatsUtils::GetGermanLanguageMetricValue(const StatId& keyHash)
{
	statAssert(StatsInterface::IsKeyValid(keyHash));

	float fnum1;
	fnum1 = StatsInterface::GetFloatStat(keyHash);

	if (STAT_AVERAGE_SPEED == keyHash)
	{
		return fnum1/1000;
	}

	return fnum1/1000;
}
float 
CStatsUtils::GetDefaultLanguageMetricValue(const StatId& keyHash)
{
	statAssert(StatsInterface::IsKeyValid(keyHash));

	float fnum1;
	fnum1 = StatsInterface::GetFloatStat(keyHash);

	if (STAT_AVERAGE_SPEED == keyHash)
	{
		return METERS_TO_MILES(fnum1);
	}

	return METERS_TO_MILES(fnum1);
}



// eof
