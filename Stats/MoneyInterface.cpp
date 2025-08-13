//
// MoneyInterface.cpp
//
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved.
//


// --- Include Files ------------------------------------------------------------

//Rage Header
#include <time.h>
#include "diag/output.h"
#include "diag/seh.h"
#include "system/param.h"
#include "rline/ros/rlros.h"
#include "rline/ros/rlroscommon.h"
#include "rline/socialclub/rlsocialclub.h"
#include "net/httpinterceptor.h"

//Framework Header

// Game headers
#include "Stats/stats_channel.h"
#include "Stats/StatsTypes.h"
#include "Stats/MoneyInterface.h"
#include "Stats/StatsInterface.h"
#include "Stats/StatsMgr.h"
#include "Stats/StatsDataMgr.h"
#include "Stats/StatsUtils.h"
#include "Stats/StatsSavesMgr.h"
#include "Network/Live/Livemanager.h"
#include "Network/NetworkInterface.h"
#include "Network/Live/NetworkTelemetry.h"
#include "Network/Live/NetworkTransactionTelemetry.h"
#include "Network/Shop/NetworkGameTransactions.h"
#include "Network/Cloud/Tunables.h"
#include "Event/EventNetwork.h"
#include "Event/EventGroup.h"
#include "network/Shop/NetworkShopping.h"

#if RSG_DURANGO
#include "Network/Live/Events_durango.h"
#endif

FRONTEND_STATS_OPTIMISATIONS()
SAVEGAME_OPTIMISATIONS()
NETWORK_OPTIMISATIONS()
NETWORK_SHOP_OPTIMISATIONS();

RAGE_DEFINE_SUBCHANNEL(stat, money, DIAG_SEVERITY_DEBUG3, DIAG_SEVERITY_DEBUG1)
#undef __stat_channel
#define __stat_channel stat_money

PARAM(cashNoTransferLimit, "Ignore the transfer limits");
PARAM(cashIgnoreServerSync, "Ignore the server authorative values");
XPARAM(nompsavegame);
XPARAM(sc_UseBasketSystem);

#if ENABLE_HTTP_INTERCEPTOR
PARAM(testCashPackSaveFail, "[stat_money] Using this command you can Turn on Profile stats Write Fails using Nick's Tool.");
PARAM(testCashPackSaveFailRuleName, "[stat_money] Using this command you can Turn on Profile stats Write Fails using Nick's Tool, Specify the rule name.");
#endif

//Min/Max CASH_GIFT_NEW values are -9223372036854775807 / 9223372036854775807
#define MAX_MONEY_INT64 (9223372036854775807)
#define MIN_MONEY_INT64 (-9223372036854775807) //(-9223372036854775807 - 1)

namespace MoneyInterface
{
	PARAM(sanitycheckmoney, "[stat_money] Sanity check money.");

	//Cache amount of money earnt in the last hour
	static s64 s_HighEarnerCashAmount      = 0;
	static u32 s_HighEarnerCashAmountTimer = 0;

	static u32 s_ValidateMoneyCount = 0;

	static bool s_preSaveFailed = false;

	static bool s_MemoryTampering = false;

	static bool s_EconomyFixedCrazyNumbers = false;


	bool IsCashValueValid(const s64 amount)
	{
		if (amount < 0)
		{
			statErrorf("FAILED IsCashValueValid(amount='%" I64FMT "d.')", amount);
			statAssertf(0, "FAILED IsCashValueValid(amount='%" I64FMT "d.')", amount);
			return false;
		}

		return true;
	}

	bool  IsAmountAndStatsValid(const s64 amount)
	{
		//Amount must be > 0
		if (!statVerifyf(amount >= 0, "Invalid value = '%" I64FMT "d'", amount))
			return false;

		if(amount == 0)
			return true;

		//Stats are not loaded
		if (StatsInterface::CloudFileLoadPending(STAT_MP_CATEGORY_DEFAULT))
			return false;

		//Stats Failed to load
		if (StatsInterface::CloudFileLoadFailed(STAT_MP_CATEGORY_DEFAULT))
			return false;

		return true;
	}

	s64  GetVCWalletBalance( const int character )
	{
		ASSERT_ONLY( if (!PARAM_nompsavegame.Get()) )
		statAssertf(s_ValidateMoneyCount > 0, "We haven't sync'd our PVC or EVC balance yet");

		s64 totalCash = 0;

		if (!StatsInterface::CloudFileLoadPending(STAT_MP_CATEGORY_DEFAULT) && !StatsInterface::CloudFileLoadFailed(STAT_MP_CATEGORY_DEFAULT))
		{
			const int mpSlot = (-1==character)?StatsInterface::GetCurrentMultiplayerCharaterSlot():character;
			Assert(mpSlot <= MAX_NUM_MP_CHARS);
			const int prefix = mpSlot+CHAR_MP_START;

			static CStatsDataMgr::StatsMap::Iterator s_StatIter;

			//Make sure we have the most updated iterator to stat STAT_WALLET_BALANCE
			if (!s_StatIter.IsValid() || s_StatIter.GetKey().GetHash() != STAT_WALLET_BALANCE.GetHash(prefix))
			{
				if (!StatsInterface::FindStatIterator(STAT_WALLET_BALANCE.GetHash(prefix), s_StatIter  ASSERT_ONLY(, "STAT_WALLET_BALANCE")))
					return totalCash;
			}
			statAssert(s_StatIter.GetKey().GetHash() == STAT_WALLET_BALANCE.GetHash(prefix));

			const sStatData* statdata = *s_StatIter;
			statAssert(statdata);

			if (statdata)
				totalCash = statdata->GetInt64();
		}

		statAssert(IsCashValueValid(totalCash));
		return totalCash;
	}

	s64  GetVCBankBalance( )
	{
		ASSERT_ONLY( if (!PARAM_nompsavegame.Get()) )
		statAssertf(s_ValidateMoneyCount > 0, "We haven't sync'd our PVC or EVC balance yet");

		s64 totalCash = 0;

		if (!StatsInterface::CloudFileLoadPending(STAT_MP_CATEGORY_DEFAULT) && !StatsInterface::CloudFileLoadFailed(STAT_MP_CATEGORY_DEFAULT))
		{
			static CStatsDataMgr::StatsMap::Iterator s_StatIter;

			//Make sure we have the most updated iterator to stat STAT_BANK_BALANCE
			if (!s_StatIter.IsValid() || s_StatIter.GetKey().GetHash() != STAT_BANK_BALANCE.GetHash())
			{
				if (!StatsInterface::FindStatIterator(STAT_BANK_BALANCE.GetHash(), s_StatIter ASSERT_ONLY(, "STAT_BANK_BALANCE")))
					return totalCash;
			}
			statAssert(s_StatIter.GetKey().GetHash() == STAT_BANK_BALANCE.GetHash());

			const sStatData* statdata = *s_StatIter;
			statAssert(statdata);

			if (statdata)
				totalCash = statdata->GetInt64();
		}

		statAssert(IsCashValueValid(totalCash));
		return totalCash;
	}

	s64  GetEVCBalance( )
	{
		s64 totalCash = 0;
		if (!StatsInterface::CloudFileLoadPending(STAT_MP_CATEGORY_DEFAULT) && !StatsInterface::CloudFileLoadFailed(STAT_MP_CATEGORY_DEFAULT))
		{
			static CStatsDataMgr::StatsMap::Iterator s_StatIter;

			//Make sure we have the most updated iterator to stat STAT_CLIENT_EVC_BALANCE
			if (!s_StatIter.IsValid() || s_StatIter.GetKey().GetHash() != STAT_CLIENT_EVC_BALANCE.GetHash())
			{
				if (!StatsInterface::FindStatIterator(STAT_CLIENT_EVC_BALANCE.GetHash(), s_StatIter  ASSERT_ONLY(, "STAT_CLIENT_EVC_BALANCE")))
					return totalCash;
			}
			statAssert(s_StatIter.GetKey().GetHash() == STAT_CLIENT_EVC_BALANCE.GetHash());

			const sStatData* statdata = *s_StatIter;
			statAssert(statdata);

			if (statdata)
				totalCash = statdata->GetInt64();
		}

		statAssert(IsCashValueValid(totalCash));
		return totalCash;
	}

	s64  GetPVCBalance()
	{
		s64 totalCash = 0;
		if (!StatsInterface::CloudFileLoadPending(STAT_MP_CATEGORY_DEFAULT) && !StatsInterface::CloudFileLoadFailed(STAT_MP_CATEGORY_DEFAULT))
		{
			static CStatsDataMgr::StatsMap::Iterator s_StatIter;

			//Make sure we have the most updated iterator to stat STAT_CLIENT_PVC_BALANCE
			if (!s_StatIter.IsValid() || s_StatIter.GetKey().GetHash() != STAT_CLIENT_PVC_BALANCE.GetHash())
			{
				if (!StatsInterface::FindStatIterator(STAT_CLIENT_PVC_BALANCE.GetHash(), s_StatIter  ASSERT_ONLY(, "STAT_CLIENT_PVC_BALANCE")))
					return totalCash;
			}
			statAssert(s_StatIter.GetKey().GetHash() == STAT_CLIENT_PVC_BALANCE.GetHash());

			const sStatData* statdata = *s_StatIter;
			statAssert(statdata);

			if (statdata)
				totalCash = statdata->GetInt64();
		}

		statAssert(IsCashValueValid(totalCash));
		return totalCash;
	}

	s64  GetVCBalance()
	{
#if __BANK && __ASSERT
		if (PARAM_sanitycheckmoney.Get())
		{
			const s64 totalVC = GetEVCBalance() + GetPVCBalance();

			s64 totalCharBankCash = GetVCBankBalance();
			for (int i=0; i<MAX_NUM_MP_ACTIVE_CHARS; i++)
				totalCharBankCash += GetVCWalletBalance(i);

			statAssertf(totalCharBankCash == totalVC, "Cash amounts differ, please delete all your MP characters, if you have already done that then add a class A.");
		}
#endif // __BANK && __ASSERT

		statAssert(IsCashValueValid(GetEVCBalance() + GetPVCBalance()));
		return (GetEVCBalance() + GetPVCBalance());
	}

	s64  GetPVCDailyAdditionsBalance()
	{
		s64 totalCash = 0;

		if (!StatsInterface::CloudFileLoadPending(STAT_MP_CATEGORY_DEFAULT) && !StatsInterface::CloudFileLoadFailed(STAT_MP_CATEGORY_DEFAULT))
		{
			static CStatsDataMgr::StatsMap::Iterator s_StatIter;
			static StatId s_statId("PVC_DAILY_ADDITIONS");

			//Make sure we have the most updated iterator to stat PVC_DAILY_ADDITIONS
			if (!s_StatIter.IsValid() || s_StatIter.GetKey().GetHash() != s_statId.GetHash())
			{
				if (!StatsInterface::FindStatIterator(s_statId.GetHash(), s_StatIter  ASSERT_ONLY(, "PVC_DAILY_ADDITIONS")))
					return totalCash;
			}
			statAssert(s_StatIter.GetKey().GetHash() == s_statId.GetHash());

			const sStatData* statdata = *s_StatIter;
			statAssert(statdata);

			if (statdata)
				totalCash = statdata->GetInt64();
		}

		statAssert(IsCashValueValid(totalCash));
		return totalCash;
	}


	s64 GetVCDailyTransferBalance()
	{
		s64 totalCash = 0;

		if (!StatsInterface::CloudFileLoadPending(STAT_MP_CATEGORY_DEFAULT) && !StatsInterface::CloudFileLoadFailed(STAT_MP_CATEGORY_DEFAULT))
		{
			static CStatsDataMgr::StatsMap::Iterator s_StatIter;
			static StatId s_statId("VC_DAILY_TRANSFERS");

			//Make sure we have the most updated iterator to stat PVC_DAILY_TRANSFERS
			if (!s_StatIter.IsValid() || s_StatIter.GetKey().GetHash() != s_statId.GetHash())
			{
				if (!StatsInterface::FindStatIterator(s_statId.GetHash(), s_StatIter  ASSERT_ONLY(, "VC_DAILY_TRANSFERS")))
					return totalCash;
			}
			statAssert(s_StatIter.GetKey().GetHash() == s_statId.GetHash());

			const sStatData* statdata = *s_StatIter;
			statAssert(statdata);

			if (statdata)
				totalCash = statdata->GetInt64();
		}

		statAssert(IsCashValueValid(totalCash));
		return totalCash;
	}


	s64  GetPVCDailyTransfersBalance()
	{
		s64 totalCash = 0;

		if (!StatsInterface::CloudFileLoadPending(STAT_MP_CATEGORY_DEFAULT) && !StatsInterface::CloudFileLoadFailed(STAT_MP_CATEGORY_DEFAULT))
		{
			static CStatsDataMgr::StatsMap::Iterator s_StatIter;
			static StatId s_statId("PVC_DAILY_TRANSFERS");

			//Make sure we have the most updated iterator to stat PVC_DAILY_TRANSFERS
			if (!s_StatIter.IsValid() || s_StatIter.GetKey().GetHash() != s_statId.GetHash())
			{
				if (!StatsInterface::FindStatIterator(s_statId.GetHash(), s_StatIter  ASSERT_ONLY(, "PVC_DAILY_TRANSFERS")))
					return totalCash;
			}
			statAssert(s_StatIter.GetKey().GetHash() == s_statId.GetHash());

			const sStatData* statdata = *s_StatIter;
			statAssert(statdata);

			if (statdata)
				totalCash = statdata->GetInt64();
		}

		statAssert(IsCashValueValid(totalCash));
		return totalCash;
	}

	float GetUSDEDailyAdditionsBalance()
	{
		float totalCash = 0;

		if (!StatsInterface::CloudFileLoadPending(STAT_MP_CATEGORY_DEFAULT) && !StatsInterface::CloudFileLoadFailed(STAT_MP_CATEGORY_DEFAULT))
		{
			static CStatsDataMgr::StatsMap::Iterator s_StatIter;
			static StatId s_statId("PVC_USDE");

			//Make sure we have the most updated iterator to stat PVC_USDE
			if (!s_StatIter.IsValid() || s_StatIter.GetKey().GetHash() != s_statId.GetHash())
			{
				if (!StatsInterface::FindStatIterator(s_statId.GetHash(), s_StatIter  ASSERT_ONLY(, "PVC_USDE")))
					return totalCash;
			}
			statAssert(s_StatIter.GetKey().GetHash() == s_statId.GetHash());

			const sStatData* statdata = *s_StatIter;
			statAssert(statdata);

			if (statdata)
				totalCash = (statdata->GetFloat());
		}

		return totalCash;
	}

	float  GetPXRValue( )
	{
		float PXR_value = 0.0f;

		if (!StatsInterface::CloudFileLoadPending(STAT_MP_CATEGORY_DEFAULT) && !StatsInterface::CloudFileLoadFailed(STAT_MP_CATEGORY_DEFAULT))
		{
			static CStatsDataMgr::StatsMap::Iterator s_StatIter;
			static StatId s_statId("CLIENT_PERSONAL_EXCHANGE_RATE");

			//Make sure we have the most updated iterator to stat PERSONAL_EXCHANGE_RATE
			if (!s_StatIter.IsValid() || s_StatIter.GetKey().GetHash() != s_statId.GetHash())
			{
				if (!StatsInterface::FindStatIterator(s_statId.GetHash(), s_StatIter  ASSERT_ONLY(, "CLIENT_PERSONAL_EXCHANGE_RATE")))
					return PXR_value;
			}
			statAssert(s_StatIter.GetKey().GetHash() == s_statId.GetHash());

			const sStatData* statdata = *s_StatIter;
			statAssert(statdata);

			if (statdata)
				PXR_value = statdata->GetFloat();
		}

		return PXR_value;
	}

	void UpdatePXRValue( const float usdeValue )
	{
		s64 pvcValue = MoneyInterface::GetPVCBalance( );

		if (pvcValue > 0 && usdeValue > 0.0f)
		{
			static StatId statPXR = ATSTRINGHASH("CLIENT_PERSONAL_EXCHANGE_RATE", 0x51A396AF);
			const float newPXR = pvcValue / usdeValue;
			StatsInterface::SetStatData(statPXR, newPXR);
			statDebugf1("New PXR = %f", StatsInterface::GetFloatStat( statPXR ));
		}
	}

	float  GetUSDEDailyTransfersBalance()
	{
		float USDEDailyTransfers = 0.0f;

		if (!statVerify(!StatsInterface::CloudFileLoadPending(STAT_MP_CATEGORY_DEFAULT)))
			return USDEDailyTransfers;

		if (!statVerify(!StatsInterface::CloudFileLoadFailed(STAT_MP_CATEGORY_DEFAULT)))
			return USDEDailyTransfers;

		//USDE Daily Transfers = PVC Daily Transfers out during the day / Personal Exchange Rate for that player (PXR)
		const s64 pvcDailyTransfersBalance = GetPVCDailyTransfersBalance( );
		if (pvcDailyTransfersBalance == 0)
			return 0.0f;

		const float PXR_value = GetPXRValue();
		if (PXR_value == 0.0f)
			return 0.0f;

		return ((float)pvcDailyTransfersBalance / PXR_value);
	}

	float GetUSDEBalance( )
	{
		s64 pvcValue = MoneyInterface::GetPVCBalance( );
		float PXRValue = MoneyInterface::GetPXRValue( );

		if (pvcValue > 0 && PXRValue>0.0f)
			return (pvcValue/PXRValue);

		return 0.0f;
	}

	// PURPOSE: Decrement/Increment PVC stat.
	bool  DecrementStatPVCBalance(const s64 amount, const bool changeValue)
	{
		if (!IsAmountAndStatsValid(amount))
			return false;

		//Decrement
		const s64 finalAmount = MoneyInterface::GetPVCBalance() - amount;
		if (!IsCashValueValid(finalAmount))
			return false;

		if (changeValue)
		{
			StatsInterface::SetStatData(STAT_CLIENT_PVC_BALANCE, finalAmount);
			statDebugf1("Operation: Decrement CLIENT_PVC_BALANCE - amount='%" I64FMT "d.' balance='%" I64FMT "d'.", amount, finalAmount);
		}

		return true;
	}
	bool  IncrementStatPVCBalance(const s64 amount, const bool changeValue)
	{
		if (!IsAmountAndStatsValid(amount))
			return false;

		//Increment
		const s64 finalAmount = MoneyInterface::GetPVCBalance() + amount;
		if (!IsCashValueValid(finalAmount))
			return false;

		if (changeValue)
		{
			StatsInterface::SetStatData(STAT_CLIENT_PVC_BALANCE, finalAmount);
			statDebugf1("Operation: Increment CLIENT_PVC_BALANCE - amount='%" I64FMT "d.' balance='%" I64FMT "d'.", amount, finalAmount);
		}

		return true;
	}

	// PURPOSE: Decrement/Increment EVC stat.
	bool  DecrementStatEVCBalance(const s64 amount, const bool changeValue)
	{
		if (!IsAmountAndStatsValid(amount))
			return false;

		//Decrement
		const s64 finalAmount = MoneyInterface::GetEVCBalance() - amount;
		if (!IsCashValueValid(finalAmount))
			return false;

		if (changeValue)
		{
			StatsInterface::SetStatData(STAT_CLIENT_EVC_BALANCE, finalAmount);
			statDebugf1("Operation: Decrement CLIENT_EVC_BALANCE - amount='%" I64FMT "d.' balance='%" I64FMT "d'.", amount, finalAmount);

			{//Increment
				static const StatId STAT_MPPLY_TOTAL_SVC = ATSTRINGHASH("MPPLY_TOTAL_SVC", 0x8A748CFA);
				const s64 finalAmount = StatsInterface::GetInt64Stat(STAT_MPPLY_TOTAL_SVC) + amount;
				StatsInterface::SetStatData(STAT_MPPLY_TOTAL_SVC, finalAmount);
				statAssert(IsCashValueValid(finalAmount));
				statDebugf1("Operation: Increment MPPLY_TOTAL_SVC - amount='%" I64FMT "d.' balance='%" I64FMT "d'.", amount, finalAmount);
			}
		}

		return true;
	}

	bool  IncrementStatEVCBalance(const s64 amount, const bool changeValue)
	{
		if (!IsAmountAndStatsValid(amount))
			return false;

		//Increment
		const s64 finalAmount = MoneyInterface::GetEVCBalance() + amount;
		if (!IsCashValueValid(finalAmount))
			return false;

		if (changeValue)
		{
			StatsInterface::SetStatData(STAT_CLIENT_EVC_BALANCE, finalAmount);
			statDebugf1("Operation: Increment CLIENT_EVC_BALANCE - amount='%" I64FMT "d.' balance='%" I64FMT "d'.", amount, finalAmount);

			{//Increment
				const int mpSlot = StatsInterface::GetCurrentMultiplayerCharaterSlot();
				statAssertf(mpSlot <= MAX_NUM_MP_CHARS, "Invalid mpSlot='%d'", mpSlot);
				if (mpSlot <= MAX_NUM_MP_CHARS)
				{
					const int prefix = mpSlot+CHAR_MP_START;

					const s64 finalAmount = StatsInterface::GetInt64Stat(STAT_TOTAL_EVC.GetHash(prefix)) + amount;
					StatsInterface::SetStatData(STAT_TOTAL_EVC.GetHash(prefix), finalAmount);

					statDebugf1("Operation: Increment TOTAL_EVC - prefix='%d', amount='%" I64FMT "d.' balance='%" I64FMT "d'.", prefix, amount, finalAmount);
					statAssert(IsCashValueValid(finalAmount));
				}
			}

			{//Increment
				static const StatId STAT_MPPLY_TOTAL_EVC = StatId(ATSTRINGHASH("MPPLY_TOTAL_EVC", 0xBF3CB334));
				const s64 finalAmount = StatsInterface::GetInt64Stat(STAT_MPPLY_TOTAL_EVC) + amount;
				StatsInterface::SetStatData(STAT_MPPLY_TOTAL_EVC, finalAmount);

				statDebugf1("Operation: Increment MPPLY_TOTAL_EVC - amount='%" I64FMT "d.' balance='%" I64FMT "d'.", amount, finalAmount);
				statAssert(IsCashValueValid(finalAmount));
			}
		}

		return true;
	}

	void  DecrementStatEVCBalance64(const s64 amount)
	{
		if (!StatsInterface::CloudFileLoadPending(STAT_MP_CATEGORY_DEFAULT) && !StatsInterface::CloudFileLoadFailed(STAT_MP_CATEGORY_DEFAULT))
		{
			if (statVerifyf(amount > 0, "Invalid value = '%" I64FMT "d'", amount))
			{
				{//Decrement
					const s64 finalAmount = MoneyInterface::GetEVCBalance() - amount;
					if (statVerify(finalAmount >= 0))
					{
						StatsInterface::SetStatData(STAT_CLIENT_EVC_BALANCE, finalAmount);
						statDebugf1("Operation: Decrement CLIENT_EVC_BALANCE - amount='%" I64FMT "d.' balance='%" I64FMT "d'.", amount, finalAmount);

						{//Increment
							static const StatId STAT_MPPLY_TOTAL_SVC = ATSTRINGHASH("MPPLY_TOTAL_SVC", 0x8A748CFA);
							const s64 finalAmount = StatsInterface::GetInt64Stat(STAT_MPPLY_TOTAL_SVC) + amount;
							StatsInterface::SetStatData(STAT_MPPLY_TOTAL_SVC, finalAmount);
							statDebugf1("Operation: Increment MPPLY_TOTAL_SVC - amount='%" I64FMT "d.' balance='%" I64FMT "d'.", amount, finalAmount);
						}
					}
				}
			}
		}
	}

	// PURPOSE: Decrement/Increment Bank stat.
	bool  IncrementStatBankBalance(const s64 amount, const bool changeValue)
	{
		if (!IsAmountAndStatsValid(amount))
			return false;

		//Increment
		const s64 finalAmount = MoneyInterface::GetVCBankBalance() + amount;
		if (!IsCashValueValid(finalAmount))
			return false;

		if (changeValue)
		{
			statDebugf1("Operation: Increment BANK_BALANCE - amount='%" I64FMT "d.' balance='%" I64FMT "d'.", amount, finalAmount);
			StatsInterface::SetStatData(STAT_BANK_BALANCE, finalAmount);
		}

		return true;
	}

	bool  DecrementStatBankBalance(const s64 amount, const bool changeValue)
	{
		if (!IsAmountAndStatsValid(amount))
			return false;

		//Decrement
		const s64 finalAmount = MoneyInterface::GetVCBankBalance() - amount;
		if (!IsCashValueValid(finalAmount))
			return false;

		if (changeValue)
		{
			statDebugf1("Operation: Decrement BANK_BALANCE - amount='%" I64FMT "d.' balance='%" I64FMT "d'.", amount, finalAmount);
			StatsInterface::SetStatData(STAT_BANK_BALANCE, finalAmount);
		}

		return true;
	}

	void  DecrementStatBankBalance64(const s64 amount)
	{
		if (!StatsInterface::CloudFileLoadPending(STAT_MP_CATEGORY_DEFAULT) && !StatsInterface::CloudFileLoadFailed(STAT_MP_CATEGORY_DEFAULT))
		{
			if (statVerifyf(amount > 0, "Invalid value = '%" I64FMT "d'", amount))
			{
				{//Decrement
					const s64 finalAmount = StatsInterface::GetInt64Stat(STAT_BANK_BALANCE) - amount;
					if (statVerify(finalAmount >= 0))
					{
						StatsInterface::SetStatData(STAT_BANK_BALANCE, finalAmount);
						statDebugf1("Operation: Decrement BANK_BALANCE - amount='%" I64FMT "d.', balance='%" I64FMT "d'.", amount, finalAmount);
					}
				}
			}
		}
	}


	// PURPOSE: Decrement/Increment Wallet stat.
	bool  IncrementStatWalletBalance(const s64 amount, const int character, const bool changeValue)
	{
		if (!IsAmountAndStatsValid(amount))
			return false;

		const int mpSlot = (-1==character)?StatsInterface::GetCurrentMultiplayerCharaterSlot():character;
		statAssertf(mpSlot <= MAX_NUM_MP_CHARS, "Invalid mpSlot='%d'", mpSlot);
		if (mpSlot <= MAX_NUM_MP_CHARS)
		{
			//Increment
			const int prefix = mpSlot+CHAR_MP_START;
			const s64 finalAmount = MoneyInterface::GetVCWalletBalance(mpSlot) + amount;
			if (!IsCashValueValid(finalAmount))
				return false;

			if (changeValue)
			{
				statDebugf1("Operation: Increment WALLET_BALANCE - prefix='%d', amount='%" I64FMT "d.' balance='%" I64FMT "d'.", prefix, amount, finalAmount);
				StatsInterface::SetStatData(STAT_WALLET_BALANCE.GetHash(prefix), finalAmount);
			}

			return true;
		}

		return false;
	}

	bool  DecrementStatWalletBalance(const s64 amount, const int character, const bool changeValue)
	{
		if (!IsAmountAndStatsValid(amount))
			return false;

		const int mpSlot = (-1==character)?StatsInterface::GetCurrentMultiplayerCharaterSlot():character;
		statAssertf(mpSlot <= MAX_NUM_MP_CHARS, "Invalid mpSlot='%d'", mpSlot);
		if (mpSlot <= MAX_NUM_MP_CHARS)
		{
			//Decrement
			const int prefix = mpSlot+CHAR_MP_START;
			const s64 finalAmount = MoneyInterface::GetVCWalletBalance(mpSlot) - amount;
			if (!IsCashValueValid(finalAmount))
				return false;

			if (changeValue)
			{
				StatsInterface::SetStatData(STAT_WALLET_BALANCE.GetHash(prefix), finalAmount);
				statDebugf1("Operation: Decrement WALLET_BALANCE - prefix='%d', amount='%" I64FMT "d.' balance='%" I64FMT "d'.", prefix, amount, finalAmount);
			}

			return true;
		}

		return false;
	}

	void  DecrementStatWalletBalance64(const s64 amount, const int character)
	{
		if (!StatsInterface::CloudFileLoadPending(STAT_MP_CATEGORY_DEFAULT) && !StatsInterface::CloudFileLoadFailed(STAT_MP_CATEGORY_DEFAULT))
		{
			const int mpSlot = (-1==character)?StatsInterface::GetCurrentMultiplayerCharaterSlot():character;
			statAssertf(mpSlot <= MAX_NUM_MP_CHARS, "Invalid mpSlot='%d'", mpSlot);

			if (mpSlot <= MAX_NUM_MP_CHARS)
			{
				if (statVerifyf(amount > 0, "Invalid value = '%" I64FMT "d'", amount))
				{
					{//Decrement
						const int prefix = mpSlot+CHAR_MP_START;
						const s64 finalAmount = StatsInterface::GetInt64Stat(STAT_WALLET_BALANCE.GetHash(prefix)) - amount;
						if (statVerify(finalAmount >= 0))
						{
							StatsInterface::SetStatData(STAT_WALLET_BALANCE.GetHash(prefix), finalAmount);
							statDebugf1("Operation: Decrement BANK_BALANCE - prefix='%d', amount='%" I64FMT "d.' balance='%" I64FMT "d'.", prefix, amount, finalAmount);
						}
					}
				}
			}
		}
	}

	// PURPOSE: amount of PVC added (through purchase or PVC gift receipt) by a player in 1 day.
	bool  IncrementStatPVCDailyAdditions(const s64 amount, const bool changeValue)
	{
		if (!IsAmountAndStatsValid(amount))
			return false;

		//Increment
		const s64 finalAmount = MoneyInterface::GetPVCDailyAdditionsBalance() + amount;
		if (!IsCashValueValid(finalAmount))
			return false;

		if (changeValue)
		{
			StatsInterface::SetStatData(STAT_PVC_DAILY_ADDITIONS, finalAmount);
			statDebugf1("Operation: Increment PVC_DAILY_ADDITIONS - amount='%" I64FMT "d.' balance='%" I64FMT "d'.", amount, finalAmount);
		}

		return true;
	}

	// PURPOSE: amount of PVC transferred out by a player to other players in 1 day.
	bool  IncrementStatPVCDailyTransfers(const s64 amount, const bool changeValue)
	{
		if (!IsAmountAndStatsValid(amount))
			return false;

		//Increment
		const s64 finalAmount = MoneyInterface::GetPVCDailyTransfersBalance() + amount;
		if (!IsCashValueValid(finalAmount))
			return false;

		if (changeValue)
		{
			StatsInterface::SetStatData(STAT_PVC_DAILY_TRANSFERS, finalAmount);
			statDebugf1("Operation: Increment PVC_DAILY_TRANSFERS - amount='%" I64FMT "d.' balance='%" I64FMT "d'.", amount, finalAmount);
		}

		return true;
	}

	bool IncrementStatVCDailyTransfers(const s64 amount, const bool changeValue)
	{
		if (!IsAmountAndStatsValid(amount))
			return false;

		//Increment
		const s64 finalAmount = MoneyInterface::GetVCDailyTransferBalance() + amount;
		if (!IsCashValueValid(finalAmount))
			return false;

		if (changeValue)
		{
			StatsInterface::SetStatData(STAT_VC_DAILY_TRANSFERS, finalAmount);
			statDebugf1("Operation: Increment VC_DAILY_TRANSFERS - amount='%" I64FMT "d.' balance='%" I64FMT "d'.", amount, finalAmount);
		}

		return true;
	}

	// PURPOSE: sum of $USD value of cash packs purchased plus USDE value of gifts received.
	bool  IncrementStatUSDEDailyAdditions(const float amount, const bool changeValue)
	{
		//Stats are not loaded
		if (StatsInterface::CloudFileLoadPending(STAT_MP_CATEGORY_DEFAULT))
			return false;

		//Stats Failed to load
		if (StatsInterface::CloudFileLoadFailed(STAT_MP_CATEGORY_DEFAULT))
			return false;

		if (statVerifyf(amount > 0.0f, "Invalid value = '%f'", amount))
		{
			if (changeValue)
			{
				statDebugf1("Operation: Increment Stat PVC_USDE - amount='%f'.", amount);
				StatsInterface::IncrementStat(STAT_PVC_USDE.GetHash(), amount);
			}

			return true;
		}

		return false;
	}

	void  CheckForCheaterStatsClear()
	{
#if __NET_SHOP_ACTIVE
		//We are dealing with Clearing cash server side.
		if(!NETWORK_SHOPPING_MGR.ShouldDoNullTransaction())
			return;
#endif

		//@@: range MONEYINTERFACE_CHECKFORCHEATERSTATSCLEAR {
		CProfileStats& psmgr = CLiveManager::GetProfileStatsMgr();
		if ( statVerify( psmgr.Synchronized(CProfileStats::PS_SYNC_MP )) )
		{
			//Cache well known stats iterators - Freemode CHEATER FLAG
			static CStatsDataMgr::StatsMap::Iterator s_StatIterIsCheater;

			//Make sure we have the most updated iterator to stat MPPLY_IS_CHEATER
			if (!s_StatIterIsCheater.IsValid() || s_StatIterIsCheater.GetKey().GetHash() != STAT_SCADMIN_IS_CHEATER.GetHash())
			{
				if (!StatsInterface::FindStatIterator(STAT_SCADMIN_IS_CHEATER.GetHash(), s_StatIterIsCheater  ASSERT_ONLY(, "STAT_SCADMIN_IS_CHEATER")))
					return;
			}
			statAssert(s_StatIterIsCheater.GetKey().GetHash() == STAT_SCADMIN_IS_CHEATER.GetHash());

			const sStatData* statIterIsCheater = *s_StatIterIsCheater;
			statAssert(statIterIsCheater);
			if (statIterIsCheater)
			{
				//@@: range MONEYINTERFACE_CHECKFORCHEATERSTATSCLEAR_CHECK_CHEATER_FLAG {
				bool isCheater = statIterIsCheater->GetBoolean();

				//Clear FREEMODE Cash
				if (isCheater)
				{ 
				//@@: } MONEYINTERFACE_CHECKFORCHEATERSTATSCLEAR_CHECK_CHEATER_FLAG	
					//Setup flags and posix time
					StatsInterface::SetStatData(STAT_MPPLY_CHEATER_CLEAR_TIME.GetHash(), rlGetPosixTime());

					//Reset the cheater flag
					isCheater = false;
					CStatsMgr::GetStatsDataMgr().SetStatIterData(s_StatIterIsCheater, &isCheater, sizeof(bool));

					//EVC amount
					const s64 totalCashCleared = StatsInterface::GetInt64Stat(STAT_CLIENT_EVC_BALANCE.GetHash());

					//Send event to clear EVC.
					CNetworkTelemetry::AppendMetric(MetricClearEVC(totalCashCleared));

					//CLEAR ALL EARNED CASH
					StatsInterface::SetStatData(STAT_EVC_BALANCE_CLEARED.GetHash(), totalCashCleared);
					StatsInterface::SetStatData(STAT_CLIENT_EVC_BALANCE.GetHash(), (s64)0);

					//Cash cleared from the players wallets and the bank.
					s64 totalWalletBankCashCleared = 0;

					const int mpSlot = StatsInterface::GetCurrentMultiplayerCharaterSlot();
					Assert(mpSlot <= MAX_NUM_MP_CHARS);
					const int prefix = mpSlot+CHAR_MP_START;

					//Clear first from the wallet.
					const s64 walletCash = StatsInterface::GetInt64Stat(STAT_WALLET_BALANCE.GetHash(prefix));
					s64 cash = walletCash;
					if (cash > 0)
					{
						cash = ((cash+totalWalletBankCashCleared)<=totalCashCleared ? cash : (totalCashCleared-totalWalletBankCashCleared));
						totalWalletBankCashCleared += cash;
						StatsInterface::SetStatData(STAT_WALLET_BALANCE.GetHash(prefix), walletCash-cash);
					}

					//@@: range MONEYINTERFACE_CHECKFORCHEATERSTATSCLEAR_CLEAR_WALLER {
					for (int i=CHAR_MP_START; i<=CHAR_MP_END && totalWalletBankCashCleared<totalCashCleared; i++)
					{
						if (prefix != i)
						{
							const s64 walletCash = StatsInterface::GetInt64Stat(STAT_WALLET_BALANCE.GetHash(i));
							s64 cash = walletCash;
							if (cash > 0)
							{
								cash = ((cash+totalWalletBankCashCleared)<=totalCashCleared ? cash : (totalCashCleared-totalWalletBankCashCleared));
								totalWalletBankCashCleared += cash;
								StatsInterface::SetStatData(STAT_WALLET_BALANCE.GetHash(i), walletCash-cash);
							}
						}
					} 
					//@@: } MONEYINTERFACE_CHECKFORCHEATERSTATSCLEAR_CLEAR_WALLER

					//Clear from the bank now.
					if (totalWalletBankCashCleared<totalCashCleared)
					{
						const s64 bankcash = StatsInterface::GetInt64Stat(STAT_BANK_BALANCE.GetHash());
						s64 cash = bankcash;
						if (cash > 0)
						{
							cash = ((cash+totalWalletBankCashCleared)<=totalCashCleared ? cash : (totalCashCleared-totalWalletBankCashCleared));
							totalWalletBankCashCleared += cash;
							StatsInterface::SetStatData(STAT_BANK_BALANCE.GetHash(), bankcash-cash);
						}
					}

					//Make sure we save this if there was no failures in loading.
					if (!StatsInterface::CloudFileLoadPending(STAT_MP_CATEGORY_DEFAULT) && !StatsInterface::CloudFileLoadFailed(STAT_MP_CATEGORY_DEFAULT))
					{
						StatsInterface::Save(STATS_SAVE_CLOUD, STAT_MP_CATEGORY_DEFAULT, STAT_SAVETYPE_COMMERCE_DEPOSIT, 0);
					}
				}
			}
		} 
		//@@: } MONEYINTERFACE_CHECKFORCHEATERSTATSCLEAR

	}

	void  UpdateHighEarnerCashAmount(s64 amount)
	{
		if (amount<=0)
			return;

		static int HIGH_EARNER_TIME_INTERVAL = 60*60*1000;
		int timeInterval = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("HIGH_EARNER_INTERVAL", 0x68b6a9f0), HIGH_EARNER_TIME_INTERVAL);

		statAssert(timeInterval>=0);
		if (timeInterval<0) timeInterval = 0; //This should never happen

		if (MoneyInterface::GetHighEarnerTimePassed() > (u32)timeInterval)
		{
			s_HighEarnerCashAmountTimer = sysTimer::GetSystemMsTime();
			s_HighEarnerCashAmount      = amount;
		}
		else
		{
			s_HighEarnerCashAmount += amount;
		}

		static int HIGH_EARNER_VALUE = 50000;
		s64 valueCash = (s64)Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("HIGH_EARNER_VALUE", 0xe00e3ce0), HIGH_EARNER_VALUE);

		//Flag as HighEarner
		if (valueCash>0 && s_HighEarnerCashAmount>=valueCash && !MoneyInterface::GetPlayerIsHighEarner())
		{
			static StatId s_statIsHighEarner(ATSTRINGHASH("MPPLY_IS_HIGH_EARNER", 0xA620E732));
			StatsInterface::SetStatData(s_statIsHighEarner, true);
		}
	}

	u32 GetHighEarnerTimePassed( )
	{
		const u32 currTime = sysTimer::GetSystemMsTime();
		if (currTime>s_HighEarnerCashAmountTimer)
		{
			return (currTime-s_HighEarnerCashAmountTimer);
		}

		return 0;
	}

	bool  GetPlayerIsHighEarner( )
	{
		if (statVerify(!StatsInterface::CloudFileLoadPending(STAT_MP_CATEGORY_DEFAULT)) && statVerify(!StatsInterface::CloudFileLoadFailed(STAT_MP_CATEGORY_DEFAULT)))
		{
			static StatId s_statIsHighEarner(ATSTRINGHASH("MPPLY_IS_HIGH_EARNER", 0xA620E732));
			static CStatsDataMgr::StatsMap::Iterator s_StatIterIsHighEarner;

			//Make sure we have the most updated iterator to stat MPPLY_IS_CHEATER
			if (!s_StatIterIsHighEarner.IsValid() || s_StatIterIsHighEarner.GetKey().GetHash() != s_statIsHighEarner.GetHash())
			{
				if (!StatsInterface::FindStatIterator(s_statIsHighEarner.GetHash(), s_StatIterIsHighEarner  ASSERT_ONLY(, "MPPLY_IS_CHEATER")))
					return false;
			}
			statAssert(s_StatIterIsHighEarner.GetKey().GetHash() == s_statIsHighEarner.GetHash());

			const sStatData* statIterHighEarner = *s_StatIterIsHighEarner;
			statAssert(statIterHighEarner);
			if (statIterHighEarner)
			{
				return statIterHighEarner->GetBoolean();
			}
		}

		return false;
	}

	void  UpdateDailyStatistics( )
	{
		static u32 s_PreviousFrame = 0;
		if (s_PreviousFrame != fwTimer::GetFrameCount())
		{
			static StatId s_statDailyTimestamp("VC_DAILY_TIMESTAMP");
			static CStatsDataMgr::StatsMap::Iterator s_StatIterDailytimestamp;

			//Make sure we have the most updated iterator to stat MPPLY_IS_CHEATER
			if (!s_StatIterDailytimestamp.IsValid() || s_StatIterDailytimestamp.GetKey().GetHash() != s_statDailyTimestamp.GetHash())
			{
				if (!StatsInterface::FindStatIterator(s_statDailyTimestamp.GetHash(), s_StatIterDailytimestamp  ASSERT_ONLY(, "MPPLY_IS_CHEATER")))
					return;
			}
			statAssert(s_StatIterDailytimestamp.GetKey().GetHash() == s_statDailyTimestamp.GetHash());

			const sStatData* statIterDailyTimestamp = *s_StatIterDailytimestamp;
			statAssert(statIterDailyTimestamp);
			if (statIterDailyTimestamp)
			{
				u64 currPosixTime = 0;
				if (GetClearDailyStatistics(statIterDailyTimestamp, currPosixTime))
				{
					statWarningf("UpdateDailyStatistics( ) - Clearing all DAILY stats.");
					StatsInterface::SetStatData(STAT_PVC_DAILY_ADDITIONS,  (s64)0);
					StatsInterface::SetStatData(STAT_PVC_DAILY_TRANSFERS,  (s64)0);
					StatsInterface::SetStatData(STAT_VC_DAILY_TRANSFERS, (s64)0);
					StatsInterface::SetStatData(STAT_PVC_USDE, 0.0f);
				}

				StatsInterface::SetStatData(s_statDailyTimestamp, currPosixTime);
			}
		}
	}

	bool  GetClearDailyStatistics(const sStatData* statIterDailyTimestamp, u64& currPosixTime)
	{
		if (!statVerify(statIterDailyTimestamp))
			return false;

		static const u64 EST_DIFF = 18000; //EST difference to GMT
		u64 posixTime = currPosixTime = (rlGetPosixTime()-EST_DIFF)*1000;
		const u64 previousPosixTime = statIterDailyTimestamp->GetUInt64();

		s32 previousdays = CStatsUtils::ConvertToDays(previousPosixTime);
		s32 currdays     = CStatsUtils::ConvertToDays(posixTime);
		if (previousdays != currdays)
			return true;

		s32 hours    = CStatsUtils::ConvertToHours(previousPosixTime);
		s32 hourDiff = CStatsUtils::ConvertToHours(posixTime-previousPosixTime);
		if (hours+hourDiff >= 23)
			return true;

		return false;
	}

	bool  CanTransferCash(const s64 amount, const float totalusde)
	{
		statDebugf1("Can transfer money:");

		if (!rlRos::HasPrivilege(NetworkInterface::GetLocalGamerIndex(), RLROS_PRIVILEGEID_TRANSFERVC))
		{
			statErrorf("Can not buy money because RLROS_PRIVILEGEID_TRANSFERVC is FALSE.");
			return false;
		}

#if !__FINAL
		if (PARAM_cashNoTransferLimit.Get())
		{
			return true;
		}
#endif
		const int VC_DAILY_TRANSFERS_MAX = 5000;
		int dailyVCAmount = VC_DAILY_TRANSFERS_MAX;
		statVerify( Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("VC_DAILY_TRANSFERS_MAX", 0x78404b26), dailyVCAmount) );

		statDebugf1(".................................  VC = '%" I64FMT "d'", amount);
		statDebugf1("........... VC daily Transfer balance = '%" I64FMT "d'", MoneyInterface::GetVCDailyTransferBalance());
		statDebugf1("....... MAX VC daily Transfer balance = '%d'", dailyVCAmount);

		if (MoneyInterface::GetVCDailyTransferBalance()+amount > (s64)(dailyVCAmount))
		{
			statErrorf("FAILED to transfer - daily VC limit reached balance='%" I64FMT "d' + amount='%" I64FMT "d' > dailyAmountMax='%d'.", MoneyInterface::GetVCDailyTransferBalance(), amount, dailyVCAmount);
			return false;
		}

		//USDE Daily Transfer Out Limit
		if (totalusde > 0.0f)
		{
			float maxUSDETranfers = 1000.0f;
			Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("PVC_DAILY_USDE_TRANSFERS_MAX", 0xc15ee223), maxUSDETranfers);

			statDebugf1("................................  USDE = '%f'", totalusde);
			statDebugf1(".......... USDE daily Transfer balance = '%f'", MoneyInterface::GetUSDEDailyTransfersBalance());
			statDebugf1("...... MAX USDE daily Transfer balance = '%f'", maxUSDETranfers);

			if (MoneyInterface::GetUSDEDailyTransfersBalance() + totalusde > maxUSDETranfers)
			{
				statErrorf("FAILED to transfer - daily USDE limit reached balance='%f' + amount='%f' > dailyAmount='%f'.", MoneyInterface::GetUSDEDailyTransfersBalance(), totalusde, maxUSDETranfers);
				return false;
			}
		}
		
		return true;
	}

	int  GetAmountAvailableForTransfer()
	{
		const int VC_DAILY_TRANSFERS_MAX = 5000;
		int dailyVCAmount = VC_DAILY_TRANSFERS_MAX;
		statVerify( Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("VC_DAILY_TRANSFERS_MAX", 0x78404b26), dailyVCAmount) );

		statDebugf1("GetAmountAvailableForTransfer:");
		statDebugf1("........... VC daily Transfer balance = '%" I64FMT "d'", MoneyInterface::GetVCDailyTransferBalance());
		statDebugf1("....... MAX VC daily Transfer balance = '%d'", dailyVCAmount);

		if (MoneyInterface::GetVCDailyTransferBalance()>(s64)(dailyVCAmount))
			return 0;

		return ((int)((s64)(dailyVCAmount) - MoneyInterface::GetVCDailyTransferBalance()));
	}


	bool CanBuyCash(const s64 /*amount*/, const float totalusde)
	{
		if ( !SavegameCanBuyCashProducts() )
		{
			return false;
		}

		statDebugf1("Can buy money:");

		if (!rlRos::HasPrivilege(NetworkInterface::GetLocalGamerIndex(), RLROS_PRIVILEGEID_PURCHASEVC))
		{
			statErrorf("Can not buy money because RLROS_PRIVILEGEID_PURCHASEVC is FALSE.");
			return false;
		}

		//Make sure we update this once.
		MoneyInterface::UpdateDailyStatistics( );

		if (IsOverDailyUSDELimit(totalusde))
		{			
			return false;
		}
		
		if (IsOverUSDEBalanceLimit(totalusde))
		{
			return false;
		}
 
		return true;
	}

	bool  IsSavegameLoaded()
	{
		CStatsSavesMgr& saveMgr = CStatsMgr::GetStatsDataMgr().GetSavesMgr();

		const int selectedSlot = StatsInterface::GetCurrentMultiplayerCharaterSlot() + 1;

		if (saveMgr.IsLoadPending(STAT_MP_CATEGORY_DEFAULT) || saveMgr.IsLoadPending(selectedSlot))
		{
			gnetError("SavegameCanBuyCashProducts() - FAILED: Load Pending for STAT_MP_CATEGORY_DEFAULT or '%d'.", selectedSlot);
			return false;
		}

		if (saveMgr.CloudLoadFailed(STAT_MP_CATEGORY_DEFAULT) || saveMgr.CloudLoadFailed(selectedSlot))
		{
			gnetError("SavegameCanBuyCashProducts() - FAILED: Load Failed for STAT_MP_CATEGORY_DEFAULT or '%d'.", selectedSlot);
			return false;
		}

		if (!CLiveManager::GetProfileStatsMgr().Synchronized(CProfileStats::PS_SYNC_MP))
		{
			gnetError("SavegameCanBuyCashProducts() - FAILED: NEED TO SYNCHRONIZE PROFILE STATS BEFORE.");
			return false;
		}

		return true;
	}

	bool  SavegameCanBuyCashProducts( )
	{
		CStatsSavesMgr& saveMgr = CStatsMgr::GetStatsDataMgr().GetSavesMgr();

		if (saveMgr.IsLoadPending(STAT_MP_CATEGORY_DEFAULT))
		{
			gnetError("SavegameCanBuyCashProducts() - FAILED: Load Pending for STAT_MP_CATEGORY_DEFAULT.");
			return false;
		}

		if (saveMgr.CloudLoadFailed(STAT_MP_CATEGORY_DEFAULT))
		{
			gnetError("SavegameCanBuyCashProducts() - FAILED: Load Failed for STAT_MP_CATEGORY_DEFAULT.");
			return false;
		}

		if (saveMgr.CloudSaveFailed(STAT_MP_CATEGORY_DEFAULT))
		{
			gnetError("SavegameCanBuyCashProducts() - FAILED: Save Failed for STAT_MP_CATEGORY_DEFAULT.");
			return false;
		}

		const int selectedSlot = StatsInterface::GetCurrentMultiplayerCharaterSlot() + 1;
		if (saveMgr.CloudSaveFailed( selectedSlot ))
		{
			gnetError("SavegameCanBuyCashProducts() - FAILED: Save Failed for %d.", selectedSlot);
			return false;
		}

		//for(int i=STAT_MP_CATEGORY_DEFAULT; i<=STAT_MP_CATEGORY_MAX ; i++)
		//{
		//	if(saveMgr.CloudSaveFailed( i ))
		//	{
		//		gnetWarning("SavegameCanBuyCashProducts() - FAILED: Save Failed for %d.", i);
		//		return false;
		//	}
		//}

		CProfileStats& profileStatsMgr = CLiveManager::GetProfileStatsMgr();
		if (profileStatsMgr.FlushFailed())
		{
			gnetError("SavegameCanBuyCashProducts() - FAILED: Multiplayer Profile Stats Flush FAILED.");
			return false;
		}

		if (!profileStatsMgr.Synchronized(CProfileStats::PS_SYNC_MP))
		{
			gnetError("SavegameCanBuyCashProducts() - FAILED: NEED TO SYNCHRONIZE PROFILE STATS BEFORE.");
			return false;
		}

		return true;
	}

	bool  CanBuyCashProducts(const CashProductBatches& productBatches)
	{
		if (!SavegameCanBuyCashProducts())
		{
			gnetError("CanBuyCashProducts() - FAILED: SavegameCanBuyCashProducts().");
			return false;
		}

		const CCashProductsData::CCashProductsList& productsList = CashPackInfo::GetInstance().GetProductsList();

		s64 totalamount = 0;
		float totalusde = 0;
		atString skuPackName;
		for (int i=0; i<productBatches.GetCount(); i++)
		{
			for (int j=0; j<productsList.GetCount(); j++)
			{
				skuPackName.Reset();
				if (productsList[j].GetSKUPackName(skuPackName) && atStringHash(skuPackName.c_str()) == productBatches[i].m_id)
				{
					totalamount += productsList[j].m_Value * productBatches[i].m_Quantity;
					totalusde   += productsList[j].m_USDE * productBatches[i].m_Quantity;
					break;
				}
			}
		}

		return CanBuyCash(totalamount, totalusde);
	}

    bool IsCashProductInData( const atHashString& product )
    {
        const CCashProductsData::CCashProductsList& productsList = CashPackInfo::GetInstance().GetProductsList();
        atString skuPackName;

        for (int i=0; i<productsList.GetCount(); i++)
        {
            skuPackName.Reset();
            if (productsList[i].GetSKUPackName(skuPackName) && atStringHash(skuPackName.c_str()) == product)
            {
                return true;
            }
        }

        return false;
    }

	bool  PreConsumeCashProduct(const atHashString& product, int numPacks)
	{
		if (!IsCashProductInData( product ))
		{
			statAssertf(false, "PreConsumeCashProduct() - Fail check for IsCashProductInData() - Failed to consume product '%s : %u', product not found.", product.GetCStr()?product.GetCStr():"Unknown", product.GetHash());
			return false;
		}

		if (!SavegameCanBuyCashProducts())
		{
			statAssertf(false, "PreConsumeCashProduct() - Fail check for SavegameCanBuyCashProducts() - Failed to consume product '%s : %u', product not found.", product.GetCStr()?product.GetCStr():"Unknown", product.GetHash());
			return false;
		}

		const CCashProductsData::CCashProductsList& productsList = CashPackInfo::GetInstance().GetProductsList();

		//[*] In the case of purchase, new USD addition value is looked up from a table showing PVC value & real money 
		//	price (in $USD only) of each cash pack at the time of the transaction.
		s64       amount = 0;
		float usdeAmount = 0;

		atString skuPackName;
		for (int i=0; i<productsList.GetCount(); i++)
		{
			skuPackName.Reset();
			if (productsList[i].GetSKUPackName(skuPackName) && atStringHash(skuPackName.c_str()) == product)
			{
				amount     = productsList[i].m_Value * numPacks;
				usdeAmount = productsList[i].m_USDE * numPacks;
				break;
			}
		}

		if (!statVerify(amount != 0 && usdeAmount != 0))
		{
			statAssertf(false, "PreConsumeCashProduct() - Fail check for (amount != 0 && usdeAmount != 0) - Failed to consume product '%s : %u', product not found.", product.GetCStr()?product.GetCStr():"Unknown", product.GetHash());
			return false;
		}

		static StatId pendingAmount("CASHPACK_AMOUNT_PENDING");
		StatsInterface::IncrementStat(pendingAmount, (float)amount);

		static StatId pendingUsde("CASHPACK_USDE_PENDING");
		StatsInterface::IncrementStat(pendingUsde, usdeAmount);

		return true;
	}

	bool  ClearConsumeCashProduct(const atHashString& product, int numPacks)
	{
		if (!IsCashProductInData( product ))
		{
			statAssertf(false, "ClearConsumeCashProduct() - Fail check for IsCashProductInData() - Failed to consume product '%s : %u', product not found.", product.GetCStr()?product.GetCStr():"Unknown", product.GetHash());
			return false;
		}

		const CCashProductsData::CCashProductsList& productsList = CashPackInfo::GetInstance().GetProductsList();

		//[*] In the case of purchase, new USD addition value is looked up from a table showing PVC value & real money 
		//	price (in $USD only) of each cash pack at the time of the transaction.
		s64       amount = 0;
		float usdeAmount = 0;

		atString skuPackName;
		for (int i=0; i<productsList.GetCount(); i++)
		{
			skuPackName.Reset();
			if (productsList[i].GetSKUPackName(skuPackName) && atStringHash(skuPackName.c_str()) == product)
			{
				amount     = productsList[i].m_Value * numPacks;
				usdeAmount = productsList[i].m_USDE * numPacks;
				break;
			}
		}

		if (!statVerify(amount != 0 && usdeAmount != 0))
		{
			statAssertf(false, "ClearConsumeCashProduct() - Fail check for (amount != 0 && usdeAmount != 0) - Failed to consume product '%s : %u', product not found.", product.GetCStr()?product.GetCStr():"Unknown", product.GetHash());
			return false;
		}

		static StatId pendingAmount("CASHPACK_AMOUNT_PENDING");
		StatsInterface::DecrementStat(pendingAmount, (float)amount);
		statDebugf1("Decrement CASHPACK_AMOUNT_PENDING by '%f' - Current Value is '%ld'.", (float)amount, StatsInterface::GetInt64Stat(pendingAmount));

		static StatId pendingUsde("CASHPACK_USDE_PENDING");
		StatsInterface::DecrementStat(pendingUsde, usdeAmount);
		statDebugf1("Decrement CASHPACK_USDE_PENDING by '%f' - Current Value is '%f'.", usdeAmount, StatsInterface::GetFloatStat(pendingUsde));

		return true;
	}

	bool  StartPreConsumeSave( )
	{
		if (!SavegameCanBuyCashProducts())
		{
			statAssertf(false,"StartPreConsumeSave( ) : SavegameCanBuyCashProducts() - Failed.");
			return false;
		}

		StatsInterface::Save(STATS_SAVE_CLOUD, STAT_MP_CATEGORY_DEFAULT, STAT_SAVETYPE_COMMERCE_DEPOSIT, 0);

		//Save must be pending 
		s_preSaveFailed = MoneyInterface::CheckPreConsumeSaveDone();

		return true;
	}

	bool  CheckPreConsumeSaveDone( )
	{
		CStatsSavesMgr& saveMgr = CStatsMgr::GetStatsDataMgr().GetSavesMgr( );
		if (saveMgr.IsSaveInProgress())
			return false;

		if (CLiveManager::GetProfileStatsMgr().PendingFlush())
			return false;

		if (CLiveManager::GetProfileStatsMgr().PendingMultiplayerFlushRequest())
			return false;

		return true;
	}

	bool  CheckPreConsumeSaveSuccessful( )
	{
		if (s_preSaveFailed)
			return false;

		return SavegameCanBuyCashProducts( );
	}

#if RSG_PC

	bool  ConsumeCashProduct(const s64 amount)
	{
		if (!SavegameCanBuyCashProducts( ))
		{
			statErrorf("ConsumeCashProduct( ) : SavegameCanBuyCashProducts() - Failed.");
			statAssertf(false,"ConsumeCashProduct( ) : SavegameCanBuyCashProducts() - Failed.");
		}

		//Generate script event
		//Need to move this event out of this function and trigger it with a server event
		CEventNetworkCashTransactionLog tEvent(-1, CEventNetworkCashTransactionLog::CASH_TRANSACTION_STORE, CEventNetworkCashTransactionLog::CASH_TRANSACTION_DEPOSIT, (int)amount, rlGamerHandle());
		GetEventScriptNetworkGroup()->Add(tEvent);

		StatsInterface::IncrementStat(STAT_MPPLY_STORE_TOTAL_MONEY_BOUGHT, static_cast<float>(amount));

		//Save last VC purchase time
		u64 posixTime = rlGetPosixTime();
		StatsInterface::SetStatData(STAT_MPPLY_STORE_PURCHASE_POSIX_TIME, posixTime, sizeof(u64));

		return true;
	}

#else //RSG_PC

	bool  ConsumeCashProduct(const atHashString& product, int numPacks, int numStoreEntries)
	{
		if (!SavegameCanBuyCashProducts( ))
		{
			statAssertf(false,"ConsumeCashProduct( ) : SavegameCanBuyCashProducts() - Failed.");
			return false;
		}

		const CCashProductsData::CCashProductsList& productsList = CashPackInfo::GetInstance().GetProductsList();

		//[*] In the case of purchase, new USD addition value is looked up from a table showing PVC value & real money 
		//	price (in $USD only) of each cash pack at the time of the transaction.
		s64       amount = 0;
		float usdeAmount = 0;
		const char* packName = NULL;

		atString skuPackName;
		for (int i=0; i<productsList.GetCount(); i++)
		{
			skuPackName.Reset();
			if (productsList[i].GetSKUPackName(skuPackName) && atStringHash(skuPackName.c_str()) == product)
			{
				amount     = productsList[i].m_Value * numPacks;
				usdeAmount = productsList[i].m_USDE * numPacks;
				packName   = productsList[i].m_PackName;
				break;
			}
		}

		if (!statVerify(amount != 0 && usdeAmount != 0 && packName != NULL))
		{
			statAssertf(false, "ConsumeCashProduct( ) : FAILED (amount != 0 && usdeAmount != 0) - Failed to consume product '%s : %u', product not found.", product.GetCStr()?product.GetCStr():"Unknown", product.GetHash());
			return false;
		}

		MoneyInterface::UpdateDailyStatistics( );

#if ENABLE_HTTP_INTERCEPTOR
		if (PARAM_testCashPackSaveFail.Get())
		{
			const char* rulename = "ProfileStatsWrite";
			PARAM_testCashPackSaveFailRuleName.Get(rulename);
			statVerify( netHttpInterception::EnableRuleByName(rulename) );
		}
#endif // ENABLE_HTTP_INTERCEPTOR

		//Generate script event
		//Need to move this event out of this function and trigger it with a server event
		CEventNetworkCashTransactionLog tEvent(-1, CEventNetworkCashTransactionLog::CASH_TRANSACTION_STORE, CEventNetworkCashTransactionLog::CASH_TRANSACTION_DEPOSIT, amount, rlGamerHandle());
		GetEventScriptNetworkGroup()->Add(tEvent);
		
		//Get the current USDE value of PVC before the addtion
		float oldUSDEValue = MoneyInterface::GetUSDEBalance();

		StatsInterface::IncrementStat(STAT_MPPLY_STORE_TOTAL_MONEY_BOUGHT, static_cast<float>(amount));
		MoneyInterface::IncrementStatPVCBalance((s64)amount);
		MoneyInterface::IncrementStatBankBalance((s64)amount);
		MoneyInterface::IncrementStatPVCDailyAdditions((s64)amount);

		//USDE Daily Additions = sum of $USD value of cash packs purchased plus USDE value of gifts received.
		MoneyInterface::IncrementStatUSDEDailyAdditions( usdeAmount );

		//New PXR = (old PVC balance + new PVC addition) / (old USDE + new USD addition).
		UpdatePXRValue( oldUSDEValue + usdeAmount );

		//Save last VC purchase time
		u64 posixTime = rlGetPosixTime();
		StatsInterface::SetStatData(STAT_MPPLY_STORE_PURCHASE_POSIX_TIME, posixTime, sizeof(u64));

		//Decrement pending cash pack values to be credited.
		static StatId pendingAmount("CASHPACK_AMOUNT_PENDING");
		StatsInterface::DecrementStat(pendingAmount, (float)amount);
		static StatId pendingUsde("CASHPACK_USDE_PENDING");
		StatsInterface::DecrementStat(pendingUsde, usdeAmount);

		//NOTE: This is no longer called here to avoid repeated saves.
		//If looking for this save, look in cCommerceManager::AutoConsumeUpdate()
		//StatsInterface::TryMultiplayerSave( STAT_SAVETYPE_COMMERCE_DEPOSIT );

		//Network telemetry
		bool bHaveWeEnteredTheStoreYet = numStoreEntries > 0;
		CNetworkTelemetry::AppendMetric(MetricPurchaseVC((int)amount, usdeAmount, packName, bHaveWeEnteredTheStoreYet));

		return true;
	}

#endif //RSG_PC

	void ClearFlagForResync()
	{
		statDisplayf("ClearFlagForResync() called to prepare for another profile stat sync");
		s_ValidateMoneyCount = 0;
	}

#if !__FINAL
	void SetValidateMoneyCount()
	{
		statDisplayf("SetValidateMoneyCount() called to prepare for another profile stat wipe.");
		s_ValidateMoneyCount = 1;
	}
#endif // !__FINAL

#if __NET_SHOP_ACTIVE

	bool ApplyVirtualCashBalance( int slot, const NetworkGameTransactions::PlayerBalance& pb, NetworkGameTransactions::PlayerBalanceApplicationInfo& applicationInfo )
	{
		statDebugf1("ApplyVirtualCashBalance: slot:%d, pb: %s", slot, pb.ToString());
		rtry
		{
			//Check to make sure all of our stats are valid.
			const sStatDescription* pDesc = StatsInterface::GetStatDesc(STAT_CLIENT_EVC_BALANCE);
			rverify( pDesc && pDesc->GetSynched(), catchall,	statErrorf("Failed Desc or Sync'd: STAT_CLIENT_EVC_BALANCE") );

			pDesc = StatsInterface::GetStatDesc(STAT_CLIENT_PVC_BALANCE);
			rverify( pDesc && pDesc->GetSynched(), catchall, statErrorf("Failed Desc or Sync'd: STAT_CLIENT_PVC_BALANCE"));

			pDesc = StatsInterface::GetStatDesc(STAT_CLIENT_PXR);
			rverify( pDesc && pDesc->GetSynched(), catchall, statErrorf("Failed Desc or Sync'd: STAT_CLIENT_PXR"));

			pDesc = StatsInterface::GetStatDesc(STAT_PVC_USDE);
			rverify( pDesc && pDesc->GetSynched(), catchall, statErrorf("Failed Desc or Sync'd: STAT_PVC_USDE"));

			pDesc = StatsInterface::GetStatDesc(STAT_BANK_BALANCE);
			rverify( pDesc && pDesc->GetSynched(), catchall, statErrorf("Failed Desc or Sync'd: STAT_BANK_BALANCE"));

			//Must verify all wallets are synched.
			for (int i=0; i<MAX_NUM_MP_CHARS; i++)
			{
				const int prefix = i+CHAR_MP_START;
				u32 slotWalletStatHash = STAT_WALLET_BALANCE.GetHash(prefix);
				pDesc = StatsInterface::GetStatDesc(slotWalletStatHash);
				rverify( pDesc && pDesc->GetSynched(), catchall, statErrorf("Failed Desc or Sync'd: MP%d_STAT_WALLET_BALANCE", i));
			}

#if !__NO_OUTPUT
			//Assert if EVC+PVC does not MATCH W's+BANK
			if ((pb.m_evc + pb.m_pvc) != (pb.m_bank+pb.m_Wallets[0]+pb.m_Wallets[1]))
			{
				statErrorf("[net_shop] ApplyVirtualCashBalance - FAIL - VC does not match BANK+WALLET. pb.m_evc='%" I64FMT "d', pb.m_pvc='%" I64FMT "d', pb.m_bank='%" I64FMT "d', pb.m_Wallets[0]='%" I64FMT "d', pb.m_Wallets[1]='%" I64FMT "d'", pb.m_evc, pb.m_pvc, pb.m_bank, pb.m_Wallets[0], pb.m_Wallets[1]);
				statAssertf(0, "[net_shop] ApplyVirtualCashBalance - FAIL - VC does not match BANK+WALLET. pb.m_evc='%" I64FMT "d', pb.m_pvc='%" I64FMT "d', pb.m_bank='%" I64FMT "d', pb.m_Wallets[0]='%" I64FMT "d', pb.m_Wallets[1]='%" I64FMT "d'", pb.m_evc, pb.m_pvc, pb.m_bank, pb.m_Wallets[0], pb.m_Wallets[1]);
			}
#endif // !__NO_OUTPUT

			//EVC
			rverify(StatsInterface::SetStatData(STAT_CLIENT_EVC_BALANCE, (void*)&pb.m_evc, sizeof(pb.m_evc), STATUPDATEFLAG_DEFAULT), catchall, 
				statErrorf("Failed to set STAT_CLIENT_EVC_BALANCE"));
			statDebugf1("Set Stat EVC BALANCE - %" I64FMT "d.", StatsInterface::GetInt64Stat(STAT_CLIENT_EVC_BALANCE));

			//PVC
			rverify(StatsInterface::SetStatData(STAT_CLIENT_PVC_BALANCE, (void*)&pb.m_pvc, sizeof(pb.m_pvc),  STATUPDATEFLAG_DEFAULT), catchall, 
				statErrorf("Failed to set STAT_CLIENT_PVC_BALANCE"));
			statDebugf1("Set Stat PVC BALANCE - %" I64FMT "d.", StatsInterface::GetInt64Stat(STAT_CLIENT_PVC_BALANCE));

			//PXR
			float fltPXR = (float)pb.m_pxr;
			rverify(StatsInterface::SetStatData(STAT_CLIENT_PXR, (void*)&fltPXR, sizeof(fltPXR), STATUPDATEFLAG_DEFAULT), catchall, 
				statErrorf("Failed to set STAT_CLIENT_PXR"));

			//USDE
			float fltUSDE = (float)pb.m_usde;
			rverify(StatsInterface::SetStatData(STAT_PVC_USDE, (void*)&fltUSDE,	sizeof(fltUSDE), STATUPDATEFLAG_DEFAULT), catchall, 
				statErrorf("Failed to set STAT_PVC_USDE"));
			statDebugf1("New PXR = %f and USDE= %f", StatsInterface::GetFloatStat( STAT_CLIENT_PXR ), StatsInterface::GetFloatStat( STAT_PVC_USDE ));

			//BANK
			applicationInfo.m_bankCashDifference = StatsInterface::GetInt64Stat(STAT_BANK_BALANCE);

			rverify(StatsInterface::SetStatData(STAT_BANK_BALANCE, (void*)&pb.m_bank, sizeof(pb.m_bank),  STATUPDATEFLAG_DEFAULT), catchall, 
				statErrorf("Failed to set STAT_BANK_BALANCE"));

			applicationInfo.m_bankCashDifference -= StatsInterface::GetInt64Stat(STAT_BANK_BALANCE);

			statDebugf1("Set Stat BANK BALANCE - %" I64FMT "d.", StatsInterface::GetInt64Stat(STAT_BANK_BALANCE));

			//Wallet (for slot)
			if (slot >= 0)
			{
				const int prefix = slot+CHAR_MP_START;
				u32 slotWalletStatHash = STAT_WALLET_BALANCE.GetHash(prefix);
				applicationInfo.m_walletCashDifference = StatsInterface::GetInt64Stat(slotWalletStatHash);

				rverify( StatsInterface::SetStatData(slotWalletStatHash, (void*)&pb.m_Wallets[slot], sizeof(s64), STATUPDATEFLAG_DEFAULT), catchall, 
					statErrorf("Failed to set STAT_WALLET_BALANCE:slot[%d]", slot));

				applicationInfo.m_walletCashDifference -= StatsInterface::GetInt64Stat(slotWalletStatHash);

				statDebugf1("Set Stat WALLET[%d] BALANCE - %" I64FMT "d.", slot, StatsInterface::GetInt64Stat(slotWalletStatHash));
			}
			//Wallet (for all slots)
			else
			{
				applicationInfo.m_walletCashDifference = 0;
				for (int i=0; i<MAX_NUM_MP_CHARS; i++)
					applicationInfo.m_walletCashDifference += StatsInterface::GetInt64Stat(STAT_WALLET_BALANCE.GetHash(i+CHAR_MP_START));

				for (int i=0; i<MAX_NUM_MP_CHARS; i++)
				{
					const u32 slotWalletStatHash = STAT_WALLET_BALANCE.GetHash(i+CHAR_MP_START);
					rverify( StatsInterface::SetStatData(slotWalletStatHash, (void*)&pb.m_Wallets[i], sizeof(s64), STATUPDATEFLAG_DEFAULT), catchall, 
						statErrorf("Failed to set STAT_WALLET_BALANCE:slot[%d]", i));

					applicationInfo.m_walletCashDifference -= StatsInterface::GetInt64Stat(slotWalletStatHash);
					statDebugf1("Set Stat WALLET[%d] BALANCE - %" I64FMT "d.", i, StatsInterface::GetInt64Stat(slotWalletStatHash));
				}
			}

			applicationInfo.m_applicationSuccessful = true;

			return true;
		}
		rcatchall
		{
			//TODO should be we a full rollback here?  I don't think so.
		}

		return false;
	}
#endif //__NET_SHOP_ACTIVE

	bool SyncAndValidateBalances()
	{
#if __NET_SHOP_ACTIVE

		//We don't care about this in PC or when we use PARAM_sc_UseBasketSystem
		if(!NETWORK_SHOPPING_MGR.ShouldDoNullTransaction())
		{
			statDisplayf("\n**********************\n SyncAndValidateBalances \n**********************\n");
			statDisplayf("\n**********************\n Not ShouldDoNullTransaction() \n**********************\n");
			statDisplayf("\n**********************\n SyncAndValidateBalances \n**********************\n");
			s_ValidateMoneyCount = 1;
			return true;
		}

#endif // __NET_SHOP_ACTIVE

		statAssertf(s_ValidateMoneyCount == 0, "SyncAndValidateBalances is being called AGAIN.  That is TERRIBLE.  Tell DANC or MIGUEL");
		statAssertf(s_ValidateMoneyCount == 0, "PLEASE DON'T IGNORE THIS SyncAndValidateBalances is being called AGAIN.  That is TERRIBLE.  Tell DANC or MIGUEL");

		bool bIgnoreServerAuthSync = true;
		const bool tuneExists = Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("IGNORE_CASH_SERVER_VALUES", 0xbd11bd22), bIgnoreServerAuthSync);
		if (bIgnoreServerAuthSync || PARAM_cashIgnoreServerSync.Get())
		{
			statDisplayf("\n**********************\nCASH SERVER SYNC IS DISABLED\n**********************\n");
			statDisplayf("\n**********************\nCASH SERVER SYNC IS DISABLED\n**********************\n");
			statDisplayf("\n**********************\nCASH SERVER SYNC IS DISABLED\n**********************\n");
			s_ValidateMoneyCount = 1;
			
			//Make sure BANK+WALLET == EVC+PVC
			MakeSureClientCashMatches();

			return true;
		}
		else if (tuneExists && !bIgnoreServerAuthSync)
		{
			return SyncAndValidateServerBalances();
		}

		return false;
	}

	bool SyncAndValidateServerBalances()
	{
		//Copy over the server values to the working client values.
		static const StatId STAT_PERSONAL_EXCHANGE_RATE("PERSONAL_EXCHANGE_RATE");
		static const StatId STAT_PVC_BALANCE("PVC_BALANCE");
		static const StatId STAT_EVC_BALANCE("EVC_BALANCE");

		const s64   serverEVCBalance = StatsInterface::GetInt64Stat(STAT_EVC_BALANCE);
		const s64   serverPVCBalance = StatsInterface::GetInt64Stat(STAT_PVC_BALANCE);
		const float   serverPXRValue = StatsInterface::GetFloatStat(STAT_PERSONAL_EXCHANGE_RATE);

		statDebugf1("Received server values for EVC [ %" I64FMT "d ] and PVC [ %" I64FMT "d ] PXR: [%.5f]", serverEVCBalance, serverPVCBalance, serverPXRValue);

		if (serverEVCBalance == 0 && serverPVCBalance == 0 && serverPXRValue == 0)
		{
			statDebugf1("SyncAndValidateBalances() - SERVER SYNCH DONE - ITS ALL GOOD - SERVER VALUES ARE ZERO-ED.");
			return true;
		}

		static const StatId STAT_CLIENT_PXR("CLIENT_PERSONAL_EXCHANGE_RATE");

#if !__NO_OUTPUT
		//Get the client values pre-sync
		const s64 Client_EVCBalance = StatsInterface::GetInt64Stat(STAT_CLIENT_EVC_BALANCE);
		const s64 Client_PVCBalance = StatsInterface::GetInt64Stat(STAT_CLIENT_PVC_BALANCE);
		const float Client_Pxr = StatsInterface::GetFloatStat(STAT_CLIENT_PXR);
		statDebugf1("Client Values BEFORE sync EVC [ %" I64FMT "d ] and PVC [ %" I64FMT "d ] PXR [ %.5f ]", Client_EVCBalance, Client_PVCBalance, Client_Pxr);
#endif

		//Slam the server value on-top of the client ones.
		StatsInterface::SetStatData(STAT_CLIENT_EVC_BALANCE, serverEVCBalance, STATUPDATEFLAG_ASSERTONLINESTATS);
		StatsInterface::SetStatData(STAT_CLIENT_PVC_BALANCE, serverPVCBalance, STATUPDATEFLAG_ASSERTONLINESTATS);
		StatsInterface::SetStatData(STAT_CLIENT_PXR,		 serverPXRValue,   STATUPDATEFLAG_ASSERTONLINESTATS);

		//Flatten Server values
		StatsInterface::SetStatData(STAT_EVC_BALANCE,            0, STATUPDATEFLAG_ASSERTONLINESTATS);
		StatsInterface::SetStatData(STAT_PVC_BALANCE,            0, STATUPDATEFLAG_ASSERTONLINESTATS);
		StatsInterface::SetStatData(STAT_PERSONAL_EXCHANGE_RATE, 0, STATUPDATEFLAG_ASSERTONLINESTATS);

		//Tick up the valid flag.
		s_ValidateMoneyCount++;
		statAssert(s_ValidateMoneyCount == 1);  //It should never be more than 1

		//Make sure BANK+WALLET == EVC+PVC
		const s64 clientEVCBalance = GetEVCBalance();
		statVerify(IsCashValueValid(clientEVCBalance));

		const s64 clientPVCBalance = GetPVCBalance();
		statVerify(IsCashValueValid(clientPVCBalance));

		const s64 clientTotalVC = clientEVCBalance + clientPVCBalance;
		statVerify(IsCashValueValid(clientTotalVC));

		const s64 clientBankBalance = GetVCBankBalance();
		statVerify(IsCashValueValid(clientBankBalance));

		s64 clientWalletBalance = 0;
		for (int i=0; i<MAX_NUM_MP_CHARS; i++)
			clientWalletBalance += GetVCWalletBalance(i);
		statVerify(IsCashValueValid(clientWalletBalance));

		const s64 totalBankAndCharWalletCash = clientBankBalance + clientWalletBalance;
		statVerify(IsCashValueValid(totalBankAndCharWalletCash));

		//If they're equal, we're good.
		statAssertf(totalBankAndCharWalletCash == clientTotalVC, "Cash amounts differ, totalBankAndCharWalletCash='%" I64FMT "d' clientTotalVC='%" I64FMT "d'", totalBankAndCharWalletCash, clientTotalVC);
		if (totalBankAndCharWalletCash == clientTotalVC)
		{
			statDebugf1("SyncAndValidateBalances() - SERVER SYNCH DONE - ITS ALL GOOD.");
			return true;
		}

		//Values for BANK+WALLET overflow and something is terrible wrong. Flaten it out and move the cash to the BANK.
		if (!IsCashValueValid(totalBankAndCharWalletCash) || !IsCashValueValid(clientBankBalance) || !IsCashValueValid(clientWalletBalance))
		{
			statErrorf("SyncAndValidateBalances() - SERVER SYNCH DONE - FAILED CHECK - !IsCashValueValid() for the wallets and bank. totalBankAndCharWalletCash='%" I64FMT "d'", totalBankAndCharWalletCash);

			StatsInterface::SetStatData(STAT_BANK_BALANCE, serverEVCBalance, STATUPDATEFLAG_ASSERTONLINESTATS);

			for (int i=0; i<MAX_NUM_MP_CHARS; i++)
				StatsInterface::SetStatData(STAT_WALLET_BALANCE.GetHash(CHAR_MP0+i), 0, STATUPDATEFLAG_ASSERTONLINESTATS);

			return true;
		}

		//If our Virtual Cash (server) value is less than our local value, we need to deduct from the player.
		if (clientTotalVC < totalBankAndCharWalletCash)
		{
			//How much are off by?
			s64 cashDiff = totalBankAndCharWalletCash - clientTotalVC;
			statDebugf1("We have more Cash than the server thinks %" I64FMT "d - %" I64FMT "d = %" I64FMT "d", totalBankAndCharWalletCash, clientTotalVC, cashDiff);

			//We need to adjust.  
			//First deduct from the bank
			if (clientBankBalance > 0)
			{
				if(cashDiff <= clientBankBalance)
				{
					statDebugf1("Bank Balance can cover difference.  Bank[%" I64FMT "d] Diff[%" I64FMT "d]", clientBankBalance, cashDiff);
					DecrementStatBankBalance(cashDiff);
					cashDiff = 0;
				}
				else // the bank doesn't cover it all 
				{
					statDebugf1("Bank Balance being cleared.  Bank[%" I64FMT "d] Diff[%" I64FMT "d]", clientBankBalance, cashDiff);
					DecrementStatBankBalance(clientBankBalance);
					cashDiff = cashDiff - clientBankBalance;
					statAssertf(cashDiff > 0, "cashDiff Error after Bank clear");
				}
			} //Bank Balance.

			//Now deduct the remaining from the wallets.
			if (cashDiff > 0 && clientWalletBalance > 0)
			{
				statDebugf1("Attempting to deduct remaining diff of [%" I64FMT "d] Wallet[%" I64FMT "d]", cashDiff, clientWalletBalance);

				//First check if the balance can cover the diff
				if (cashDiff >= clientWalletBalance)
				{
					statDebugf1("Wallet Balance can't cover the Diff.  Clearing ALL wallets: Wallet[%" I64FMT "d] Diff[%" I64FMT "d]", clientWalletBalance, cashDiff);
					s64 charWalletAmt = 0;
					for (int i=0; i<MAX_NUM_MP_CHARS; i++)
					{
						charWalletAmt = GetVCWalletBalance(i);
						if (charWalletAmt > 0)
						{
							DecrementStatWalletBalance(charWalletAmt, i);
							cashDiff -= charWalletAmt;
						}
					}

				}
				else //Be selective
				{
					//Emptying wallets for chars.  Do the other slots first.
					s64 charWalletAmt = 0;
					int currentCharSlot = StatsInterface::GetCurrentMultiplayerCharaterSlot();
					for (int i=0; i<MAX_NUM_MP_CHARS && cashDiff > 0; i++)
					{
						//Skip the current slot...we do them last.
						if (i == currentCharSlot)
						{
							continue;
						}

						charWalletAmt = GetVCWalletBalance(i);
						if (charWalletAmt > 0)
						{
							//If this wallet has enough, just take that much..
							if (charWalletAmt >= cashDiff)
							{
								DecrementStatWalletBalance(cashDiff, i);
								statDebugf1("Wallet float slot %d covered Diff %" I64FMT "d", i, cashDiff);

								cashDiff = 0;	
							}
							else //Clear out the wallet.
							{
								DecrementStatWalletBalance(charWalletAmt, i);
								cashDiff -= charWalletAmt;
								statDebugf1("Still neeed to clear another wallet after slot %d.  Diff %" I64FMT "d", i, cashDiff);
							}							
						}

					}//Foreach char

					//If we still have a diff, we'll need to hit the current slot...sorry man.
					if (currentCharSlot >= 0 && cashDiff > 0)
					{
						charWalletAmt = GetVCWalletBalance(currentCharSlot);
						if (charWalletAmt > 0)
						{
							if (charWalletAmt >= cashDiff)
							{
								DecrementStatWalletBalance(cashDiff, currentCharSlot);
								statDebugf1("Wallet float slot %d covered Diff %" I64FMT "d", currentCharSlot, cashDiff);

								cashDiff = 0;	
							}
							else //Clear out the wallet.
							{
								DecrementStatWalletBalance(charWalletAmt, currentCharSlot);
								cashDiff -= charWalletAmt;
								statDebugf1("Still need to clear another wallet after slot %d.  Diff %" I64FMT "d", currentCharSlot, cashDiff);
							}
						}
					}
				}

			} //Wallet Balance

			statAssertf(cashDiff == 0, "Played didn't have enough cash to cover the difference. Diff remaining %" I64FMT "d", cashDiff );

			return cashDiff == 0;
		} //totalVC < BankWallet

		//The server thinks we have more than we do...
		if (clientTotalVC > totalBankAndCharWalletCash)
		{
			//Add it to the bank...i guess
			s64 VCDiff = clientTotalVC - totalBankAndCharWalletCash;

			statDebugf1("Server reports more VC [%" I64FMT "d] than in the player accounts [%" I64FMT "d]....adding %" I64FMT "d", clientTotalVC, totalBankAndCharWalletCash, VCDiff);

			IncrementStatBankBalance(VCDiff);

			return true;
		}

		return false;
	}

	void  FixBankAndWalletByTotalVC( )
	{
		const s64 clientEVCBalance  = GetEVCBalance();
		const s64 clientPVCBalance  = GetPVCBalance();
		const s64 clientTotalVC = clientEVCBalance + clientPVCBalance;

		s64 clientBankBalance = GetVCBankBalance();
		s64 clientWalletBalance = 0;
		for (int i=0; i<MAX_NUM_MP_CHARS; i++)
			clientWalletBalance += GetVCWalletBalance(i);

		const s64 totalBankAndCharWalletCash = clientBankBalance + clientWalletBalance;

		if (clientTotalVC == totalBankAndCharWalletCash)
			return;

		//Values are negative and overflowing
		if (clientBankBalance < 0 || clientWalletBalance < 0)
		{
			StatsInterface::SetStatData(STAT_BANK_BALANCE, clientTotalVC);
			for (int i=0; i<MAX_NUM_MP_CHARS; i++)
				StatsInterface::SetStatData(STAT_WALLET_BALANCE.GetHash(CHAR_MP0+i), 0, STATUPDATEFLAG_ASSERTONLINESTATS);
		}
		else
		{
			//We only need to correct the BANK - lets not touch the WALLET
			if (clientTotalVC > clientWalletBalance)
			{
				StatsInterface::SetStatData(STAT_BANK_BALANCE, clientTotalVC - clientWalletBalance);
			}
			//Move to BANK - move it all to the BANK
			else
			{
				StatsInterface::SetStatData(STAT_BANK_BALANCE, clientTotalVC);
				for (int i=0; i<MAX_NUM_MP_CHARS; i++)
					StatsInterface::SetStatData(STAT_WALLET_BALANCE.GetHash(CHAR_MP0+i), 0, STATUPDATEFLAG_ASSERTONLINESTATS);
			}
		}
	}

	bool  FixCrazyNumbersEconomy( )
	{
		bool result = false;

		s64 clientEVCBalance  = GetEVCBalance();
		s64 clientPVCBalance  = GetPVCBalance();
		s64 clientBankBalance = GetVCBankBalance();
		s64 clientWalletBalance = 0;
		for (int i=0; i<MAX_NUM_MP_CHARS; i++)
			clientWalletBalance += GetVCWalletBalance(i);

		s64 totalBankAndCharWalletCash = clientBankBalance + clientWalletBalance;

		//Overflow value - visible change
		const s64   newEVC  = StatsInterface::GetInt64Stat(STAT_CASH_EVC_CORRECTION);
		const s64   newPVC  = StatsInterface::GetInt64Stat(STAT_CASH_PVC_CORRECTION);
		const float newPXR  = StatsInterface::GetFloatStat(STAT_CASH_PXR_CORRECTION);
		const float newUSDE = StatsInterface::GetFloatStat(STAT_CASH_USDE_CORRECTION);

		const bool fixCrazyNumbers = (newEVC > 0 || newPVC > 0 || newPXR > 0.0f || newUSDE > 0.0f);
		if (fixCrazyNumbers)
		{
			if (newEVC > 0)
			{
				StatsInterface::SetStatData(STAT_CASH_EVC_CORRECTION, 0);
				StatsInterface::SetStatData(STAT_CLIENT_EVC_BALANCE, newEVC);
			}
			clientEVCBalance = GetEVCBalance();

			if (newPVC > 0)
			{
				StatsInterface::SetStatData(STAT_CASH_PVC_CORRECTION, 0);
				StatsInterface::SetStatData(STAT_CLIENT_PVC_BALANCE, newPVC);
			}
			clientPVCBalance = GetPVCBalance();

			if (newEVC > 0 || newPVC > 0)
			{
				FixBankAndWalletByTotalVC( );
				clientBankBalance = GetVCBankBalance();
				clientWalletBalance = 0;
				for (int i=0; i<MAX_NUM_MP_CHARS; i++)
					clientWalletBalance += GetVCWalletBalance(i);
			}

			statDebugf1("FixCrazyNumbersEconomy() - fixCrazyNumbers - newEVC='%" I64FMT "d', newPVC='%" I64FMT "d', balanceFix='%" I64FMT "d'", clientEVCBalance, clientPVCBalance, totalBankAndCharWalletCash);

			if (newPXR > 0.0f)
			{
				StatsInterface::SetStatData(STAT_CLIENT_PXR, newPXR);
				StatsInterface::SetStatData(STAT_CASH_PXR_CORRECTION, 0.0f);
				statDebugf1("FixCrazyNumbersEconomy() - fixCrazyNumbers - newPXR='%f'", newPXR);
			}

			if (newUSDE > 0.0f)
			{
				StatsInterface::SetStatData(STAT_PVC_USDE, newUSDE);
				StatsInterface::SetStatData(STAT_CASH_USDE_CORRECTION, 0.0f);
				statDebugf1("FixCrazyNumbersEconomy() - fixCrazyNumbers - newUSDE='%f'", newUSDE);
			}

			result = s_EconomyFixedCrazyNumbers = true;

			clientEVCBalance  = GetEVCBalance();
			clientPVCBalance  = GetPVCBalance();
			clientBankBalance = GetVCBankBalance();
			clientWalletBalance = 0;
			for (int i=0; i<MAX_NUM_MP_CHARS; i++)
				clientWalletBalance += GetVCWalletBalance(i);
			totalBankAndCharWalletCash = clientBankBalance + clientWalletBalance;
		}

		//Not enough money to cover current PVC - visible change. Credit BANK only
		const bool fixBankWallet = StatsInterface::GetBooleanStat(STAT_CASH_FIX_PVC_WB_CORRECTION);
		if (totalBankAndCharWalletCash < clientPVCBalance && fixBankWallet)
		{
			s64 newBankBalance = clientBankBalance + (clientPVCBalance - totalBankAndCharWalletCash);

			StatsInterface::SetStatData(STAT_BANK_BALANCE, newBankBalance);
			StatsInterface::SetStatData(STAT_CASH_FIX_PVC_WB_CORRECTION, false);

			statDebugf1("FixCrazyNumbersEconomy() -  BankBalance='%" I64FMT "d', newBankBalance='%" I64FMT "d'", clientBankBalance, newBankBalance);

			s_EconomyFixedCrazyNumbers = true;

			result = true;

			clientBankBalance = GetVCBankBalance();
			totalBankAndCharWalletCash = clientBankBalance + clientWalletBalance;
		}

		//EVC needs to be corrected - No visible change
		const bool fixEvcBalance = StatsInterface::GetBooleanStat(STAT_CASH_FIX_EVC_CORRECTION);
		if ((clientEVCBalance != (totalBankAndCharWalletCash - clientPVCBalance) || clientEVCBalance < 0) && fixEvcBalance)
		{
			s64 newEVC = totalBankAndCharWalletCash - clientPVCBalance;
			if (newEVC < 0)
				newEVC = 0;

			StatsInterface::SetStatData(STAT_CLIENT_EVC_BALANCE, newEVC);
			StatsInterface::SetStatData(STAT_CASH_FIX_EVC_CORRECTION, false);

			statDebugf1("FixCrazyNumbersEconomy() -  clientEVCBalance='%" I64FMT "d', newEVC='%" I64FMT "d'", clientEVCBalance, newEVC);
		}

		//Returns TRUE if there is a visible change
		return result;
	}

	bool  EconomyFixedCrazyNumbers( )
	{
		if (s_EconomyFixedCrazyNumbers)
		{
			statDebugf1("EconomyFixedCrazyNumbers() -  Reset s_EconomyFixedCrazyNumbers.");

			s_EconomyFixedCrazyNumbers = false;

			return true;
		}

		return false;
	}

	void  MakeSureClientCashMatches()
	{
		FixCrazyNumbersEconomy();

		//Make sure BANK+WALLET == EVC+PVC
		const s64 EVCBalance = GetEVCBalance();
		const s64 PVCBalance = GetPVCBalance();
		const s64 totalVC = EVCBalance + PVCBalance;

		const s64 BankBalance = GetVCBankBalance();
		s64 WalletBalance = 0;
		for (int i=0; i<MAX_NUM_MP_CHARS; i++)
			WalletBalance += GetVCWalletBalance(i);

		const s64 totalBankAndCharWalletCash = BankBalance + WalletBalance;

		//If they're equal, we're good.
		if (totalBankAndCharWalletCash == totalVC)
			return;

		if(PARAM_sanitycheckmoney.Get())
			statAssertf(totalBankAndCharWalletCash == totalVC, "Cash amounts differ, totalBankAndCharWalletCash='%" I64FMT "d' clientTotalVC='%" I64FMT "d'.", totalBankAndCharWalletCash, totalVC);

		//If our Virtual Cash (server) value is less than our local value, we need to deduct from the player.
		if (totalVC < totalBankAndCharWalletCash)
		{
			//How much are off by?
			s64 cashDiff = totalBankAndCharWalletCash - totalVC;
			statDebugf1("We have more Cash than the server thinks %" I64FMT "d - %" I64FMT "d = %" I64FMT "d", totalBankAndCharWalletCash, totalVC, cashDiff);

			//We need to adjust.  
			//First deduct from the bank
			if (BankBalance > 0)
			{
				if(cashDiff <= BankBalance)
				{
					statDebugf1("Bank Balance can cover difference.  Bank[%" I64FMT "d] Diff[%" I64FMT "d]", BankBalance, cashDiff);
					DecrementStatBankBalance(cashDiff);
					cashDiff = 0;
				}
				else // the bank doesn't cover it all 
				{
					statDebugf1("Bank Balance being cleared.  Bank[%" I64FMT "d] Diff[%" I64FMT "d]", BankBalance, cashDiff);
					DecrementStatBankBalance(BankBalance);
					cashDiff = cashDiff - BankBalance;
					statAssertf(cashDiff > 0, "cashDiff Error after Bank clear");
				}
			} //Bank Balance.

			//Now deduct the remaining from the wallets.
			if (cashDiff > 0 && WalletBalance > 0)
			{
				statDebugf1("Attempting to deduct remaining diff of [%" I64FMT "d] Wallet[%" I64FMT "d]", cashDiff, WalletBalance);

				//First check if the balance can cover the diff
				if (cashDiff >= WalletBalance)
				{
					statDebugf1("Wallet Balance can't cover the Diff.  Clearing ALL wallets: Wallet[%" I64FMT "d] Diff[%" I64FMT "d]", WalletBalance, cashDiff);
					s64 charWalletAmt = 0;
					for (int i=0; i<MAX_NUM_MP_CHARS; i++)
					{
						charWalletAmt = GetVCWalletBalance(i);
						if (charWalletAmt > 0)
						{
							DecrementStatWalletBalance(charWalletAmt, i);
							cashDiff -= charWalletAmt;
						}
					}

				}
				else //Be selective
				{
					//Emptying wallets for chars.  Do the other slots first.
					s64 charWalletAmt = 0;
					int currentCharSlot = StatsInterface::GetCurrentMultiplayerCharaterSlot();
					for (int i=0; i<MAX_NUM_MP_CHARS && cashDiff > 0; i++)
					{
						//Skip the current slot...we do them last.
						if (i == currentCharSlot)
						{
							continue;
						}

						charWalletAmt = GetVCWalletBalance(i);
						if (charWalletAmt > 0)
						{
							//If this wallet has enough, just take that much..
							if (charWalletAmt >= cashDiff)
							{
								DecrementStatWalletBalance(cashDiff, i);
								statDebugf1("Wallet float slot %d covered Diff %" I64FMT "d", i, cashDiff);

								cashDiff = 0;	
							}
							else //Clear out the wallet.
							{
								DecrementStatWalletBalance(charWalletAmt, i);
								cashDiff -= charWalletAmt;
								statDebugf1("Still neeed to clear another wallet after slot %d.  Diff %" I64FMT "d", i, cashDiff);
							}							
						}

					}//Foreach char

					//If we still have a diff, we'll need to hit the current slot...sorry man.
					if (currentCharSlot >= 0 && cashDiff > 0)
					{
						charWalletAmt = GetVCWalletBalance(currentCharSlot);
						if (charWalletAmt > 0)
						{
							if (charWalletAmt >= cashDiff)
							{
								DecrementStatWalletBalance(cashDiff, currentCharSlot);
								statDebugf1("Wallet float slot %d covered Diff %" I64FMT "d", currentCharSlot, cashDiff);

								cashDiff = 0;	
							}
							else //Clear out the wallet.
							{
								DecrementStatWalletBalance(charWalletAmt, currentCharSlot);
								cashDiff -= charWalletAmt;
								statDebugf1("Still neeed to clear another wallet after slot %d.  Diff %" I64FMT "d", currentCharSlot, cashDiff);
							}
						}
					}
				}

			} //Wallet Balance

			statAssertf(cashDiff == 0, "Played didn't have enough cash to cover the difference. Diff remainging %" I64FMT "d", cashDiff );
		} //totalVC < BankWallet
	}

	int GetCashPackValue( const char* productID )
	{
		const CCashProductsData::CCashProductsList& productsList = CashPackInfo::GetInstance().GetProductsList();
		for (int j=0; j<productsList.GetCount(); j++)
		{
			atString skuPackName;
			
			if ( productsList[j].GetSKUPackName(skuPackName) && atStringHash(skuPackName) == atStringHash(productID) )
			{
				return static_cast<int>(productsList[j].m_Value);
			}
		}

		return -1;
	}

	float GetCashPackUSDEValue( const char* productID )
	{
		const CCashProductsData::CCashProductsList& productsList = CashPackInfo::GetInstance().GetProductsList();
		for (int j=0; j<productsList.GetCount(); j++)
		{
			atString skuPackName;

			if ( productsList[j].GetSKUPackName(skuPackName) && atStringHash(skuPackName) == atStringHash(productID) )
			{
				return productsList[j].m_USDE;
			}
		}

		return -1;
	}

	int RequestBankCashWithdrawal( int amount )
	{
		if(statVerifyf(((s64)amount) <= MoneyInterface::GetVCBankBalance(), "Not enough money=%" I64FMT "d to transfer %d.", MoneyInterface::GetVCBankBalance(), amount))
		{
			const bool result = MoneyInterface::DecrementStatBankBalance((s64)amount, false) && MoneyInterface::IncrementStatWalletBalance((s64)amount, -1, false);
			if (result)
			{
				MoneyInterface::DecrementStatBankBalance((s64)amount);
				MoneyInterface::IncrementStatWalletBalance((s64)amount);

				//Generate script event
				CEventNetworkCashTransactionLog tEvent(0, CEventNetworkCashTransactionLog::CASH_TRANSACTION_ATM, CEventNetworkCashTransactionLog::CASH_TRANSACTION_WITHDRAW, amount, rlGamerHandle());
				GetEventScriptNetworkGroup()->Add(tEvent);

				CNetworkTelemetry::AppendMetric(MetricAllocateVC(amount));

				return amount;
			}
		}

		return 0;
	}

	bool CreditBankCashStats( int amount )
	{
		if(statVerifyf(((s64)amount) <= MoneyInterface::GetVCWalletBalance(), "Not enough money=%" I64FMT "d to transfer %d.", MoneyInterface::GetVCWalletBalance(), amount))
		{
			const bool result = MoneyInterface::DecrementStatWalletBalance((s64)amount, -1, false) && MoneyInterface::IncrementStatBankBalance((s64)amount, false);
			if (result)
			{
				MoneyInterface::DecrementStatWalletBalance((s64)amount);
				MoneyInterface::IncrementStatBankBalance((s64)amount);

				//Generate script event
				CEventNetworkCashTransactionLog tEvent(-1, CEventNetworkCashTransactionLog::CASH_TRANSACTION_ATM, CEventNetworkCashTransactionLog::CASH_TRANSACTION_DEPOSIT, amount, rlGamerHandle());
				GetEventScriptNetworkGroup()->Add(tEvent);

				//Add metric
				CNetworkTelemetry::AppendMetric(MetricRecoupVC(amount));
			}

			return result;
		}
		
		return false;
	}

	bool IsOverDailyUSDELimit( const float totalusde )
	{
		//USDE Daily Purchase Limit = $1,000 (XBL purchase limit)
		float maxUSDEDailyPurch = 1000.00f;
		Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("PVC_DAILY_PURCH_USDE_MAX", 0x5f19cabd), maxUSDEDailyPurch);

		if (MoneyInterface::GetUSDEDailyAdditionsBalance()+totalusde > maxUSDEDailyPurch)
		{
			statErrorf("IsOverDailyUSDELimit: FAILED - Daily USDE limit reached balance='%f' + amount='%f' > dailyAmount='%f'.", MoneyInterface::GetUSDEDailyAdditionsBalance(), totalusde, maxUSDEDailyPurch);
			return true;
		}
		else
		{
			statDebugf1("IsOverDailyUSDELimit: Purchase USDE='%f', USDE daily balance='%f', MAX USDE Purch Limit='%f'.", totalusde, MoneyInterface::GetUSDEDailyAdditionsBalance( ), maxUSDEDailyPurch);
		}

		return false;
	}

	bool IsOverUSDEBalanceLimit( const float totalusde )
	{
		//USDE Balance Limit = $150
		float maxUSDEBalance = 150.00f;
		Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("PVC_BANK_BALANCE_MAX", 0x65291459), maxUSDEBalance);

		statDebugf1("CanBuyCash .....Current Player USDE balance = '%f'", MoneyInterface::GetUSDEBalance( ));
		statDebugf1("CanBuyCash ........ MAX Player USDE balance = '%f'", maxUSDEBalance);

		if (MoneyInterface::GetUSDEBalance( )+totalusde > maxUSDEBalance)
		{
			statErrorf("IsOverUSDEBalanceLimit FAILED - USDE Balance reached Limit='%f' + amount='%f' > maxUSDPurch='%f'.", MoneyInterface::GetUSDEBalance( ), totalusde, maxUSDEBalance);
			return true;
		}

		return false;
	}

	s64 SpendEarnedCashFromAll( const s64 amount )
	{
		s64 amountToDeduct = amount;
		s64 amountDeducted = 0;
		//Let's get what the player's current EVC value is.
		const s64 earntCash = MoneyInterface::GetEVCBalance();

		const s64 BankBalance = MoneyInterface::GetVCBankBalance();
		s64 WalletBalance = 0;
		for (int i=0; i<MAX_NUM_MP_CHARS; i++)
			WalletBalance += MoneyInterface::GetVCWalletBalance(i);

		statDebugf1("SpendEarnedCashFromAll - Amount: [%" I64FMT "d]  EVC: [%" I64FMT "d]  AmountToDeduct: [%" I64FMT "d]   Bank:[%" I64FMT "d]  Wallets:[%" I64FMT "d]", amount, earntCash, amountToDeduct, BankBalance, WalletBalance);

		//-	The ability with each gift to specify - leave player with at least $X.
		const s64 minBalanceAmount = StatsInterface::GetInt64Stat(STAT_CASH_GIFT_MIN_BALANCE);
		if (minBalanceAmount > 0)
		{
			//Remove only what we can from the bank+wallet.
			if (minBalanceAmount <= BankBalance+WalletBalance)
			{
				const s64 maxDeduction = (BankBalance+WalletBalance) - minBalanceAmount;
				amountToDeduct = (amountToDeduct > maxDeduction) ? maxDeduction : amountToDeduct;
			}
			// Min balance is bigger to what we have so don't decrement any cash.
			else
			{
				amountToDeduct = 0;
			}

			statDebugf1("Correct amount to deduct - min balance='%" I64FMT "d', amountToDeduct='%" I64FMT "d'", minBalanceAmount, amountToDeduct);
		}

		//We can only deduct at most the EVC
		if (amountToDeduct > earntCash)
		{
			statDebugf1("Correct amount to deduct - not enough EVC='%" I64FMT "d', amountToDeduct='%" I64FMT "d'", earntCash, amountToDeduct);
			amountToDeduct = earntCash;
		}

		if (amountToDeduct <= 0)
		{
			statDebugf1("SpendEarnedCashFromAll - FAILED - amountToDeduct <= 0, amountToDeduct='%" I64FMT "d'", amountToDeduct);
			return 0;
		}

		//First deduct from the bank
		if (BankBalance > 0)
		{
			if(amountToDeduct <= BankBalance)
			{
				statDebugf1("Bank Balance can cover difference.  Bank[%" I64FMT "d] Diff[%" I64FMT "d]", BankBalance, amountToDeduct);
				DecrementStatBankBalance64(amountToDeduct);
				amountDeducted = amountToDeduct;
				amountToDeduct = 0;
			}
			else // the bank doesn't cover it all 
			{
				statDebugf1("Bank Balance being cleared.  Bank[%" I64FMT "d] Diff[%" I64FMT "d]", BankBalance, amountDeducted);
				DecrementStatBankBalance64(BankBalance);
				amountToDeduct -= BankBalance;
				amountDeducted += BankBalance;
			}
		}

		statDebugf1("Still have [%" I64FMT "d] remiaing to deduct", amountToDeduct);

		//Now deduct the remaining from the wallets.
		if (amountToDeduct > 0 && WalletBalance > 0)
		{
			//Loop through all the wallets until we find the amount to deduct
			s64 charWalletAmt = 0, charWalletAmtToDeduct = 0;
			for (int i=0; i<MAX_NUM_MP_CHARS && amountToDeduct > 0; i++)
			{
				charWalletAmt = GetVCWalletBalance(i);
				if (charWalletAmt > 0)
				{
					statDebugf1("Wallet[%d] has [%" I64FMT "d]", i, charWalletAmt);
					//If the amount remainig to deduct is less than what's in this wallet,
					//only take what we need.  Otherwise, we're gonna take it all.
					charWalletAmtToDeduct = charWalletAmt;
					if (charWalletAmt > amountToDeduct)
					{
						charWalletAmtToDeduct = amountToDeduct;
					}

					DecrementStatWalletBalance64(charWalletAmtToDeduct, i);
					amountToDeduct -= charWalletAmtToDeduct;
					amountDeducted += charWalletAmtToDeduct;
					statDebugf1("Deducting from Wallet[%d] - Deducting [%" I64FMT "d] of [%" I64FMT "d].  Total remaining to deduct [%" I64FMT "d]",i, charWalletAmtToDeduct, charWalletAmt, amountToDeduct);
				}
			}
		}

		//Since we're only taking EVC, make sure we update the EVC balance
		MoneyInterface::DecrementStatEVCBalance64(amountDeducted);

		//Flag we want profile stats flush
		CStatsMgr::GetStatsDataMgr().GetSavesMgr().FlushProfileStats( );

		return amountDeducted;
	}

	void  MemoryTampering( const bool value )
	{
		s_MemoryTampering = value;
		statDebugf1("Set MemoryTampering: value='%s'", value ? "true":"false");
	}

	bool  IsMemoryTampered( )
	{
		return s_MemoryTampering;
	}
}


// --- CCashProductsData ----------------------------------------------------------------
void  CCashProductsData::Shutdown()
{
	m_productIds.clear();
}

void CCashProductsData::Update()
{
	//If we've already done the deed, then we're good.
	if (m_received)
	{
		return;
	}

	//See if we have a valid local gamer
	int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
	if (!RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex))
	{
		return;
	}

	//Wait for a valid ticket
	const rlRosCredentials& cred = rlRos::GetCredentials(localGamerIndex);
	if(!cred.IsValid())
	{
		return;
	}

	if (!m_netStatus.Pending())
	{	
		//See if we're waiting for a retry
		if (m_retryTimer.IsRunning())
		{
			//See if the retry is up.
			if (m_retryTimer.GetElapsedMilliseconds() >= RETRY_TIME_MS)
			{
				m_retryTimer.Reset();
			}
		}

		//See if we've started it at all
		if (m_netStatus.Canceled() || m_netStatus.Succeeded() || m_netStatus.Failed())
		{
			//Handle it being done.
			if (m_netStatus.Succeeded())
			{
				//Set the atArray to the correct size based on the number we received.
                if ( m_countReceived > MAX_NUM_PRODUCTS )
                {
                    m_productIds.SetCount(0);
                    m_received = true;
                    statErrorf("Invalid number of cash products recieved from service. Count is %d.", m_countReceived);
                }
                else
                {
				    m_productIds.SetCount(m_countReceived);
				    m_received = true;
                    m_netStatus.Reset();
				    statDebugf1("Received %d Cash Pack USDE Infos", m_countReceived);
                }
			}
			else if(m_netStatus.Failed() || m_netStatus.Canceled())
			{
				//Try again if we haven't tried too many times
				if (m_numRetries < MAX_NUM_RETRIES)
				{
					statErrorf("GetCashTransactionsPackUSDEInfo FAILED.  Starting retry %d", m_numRetries);
					m_retryTimer.Start();
					m_numRetries++;
				}
				else
				{
					//We give up
					statErrorf("Failed to retrieve CashPackUSDE Info");
					m_received = true;  //Say we're received it to fully give up.
				}
			}
		}
		//If the netStatus has no status and we're not waiting for a retry, Kick it off!
		else if(!m_retryTimer.IsRunning())
		{
			statAssert((unsigned)m_productIds.GetMaxCount()== MAX_NUM_PRODUCTS);
			statVerify(rlSocialClub::GetCashTransactionsPackUSDEInfo(localGamerIndex, m_productIds.GetElements(), MAX_NUM_PRODUCTS, &m_countReceived, &m_netStatus));
		}
	}

	//
}

bool CCashProductsData::SkuCashPackInfo::GetSKUPackName( atString& outString ) const
{
    //Clear the string so we dont get unexpected concatenation
    outString.Reset();
#if __PPU
	const char* skuPrefix = CLiveManager::GetCommerceMgr().GetCommerceConsumableIdPrefix();

	outString = skuPrefix;
#endif 

	outString += m_PackName;

	return true;
}

// EOF


