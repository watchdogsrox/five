
#include "savegame_data_stats.h"
#include "savegame_data_stats_parser.h"


// Game headers
#include "control/stuntjump.h"
#include "Peds/ped.h"
#include "SaveLoad/GenericGameStorage.h"
#include "SaveLoad/savegame_channel.h"
#include "SaveLoad/savegame_data.h"
#include "stats/stats_channel.h"
#include "Stats/StatsDataMgr.h"
#include "stats/StatsInterface.h"



CBaseStatsSaveStructure::CBaseStatsSaveStructure()
{
	m_IntData.Reset();
	m_FloatData.Reset();
	m_BoolData.Reset();
	m_UInt8Data.Reset();
	m_UInt16Data.Reset();
	m_UInt32Data.Reset();
	m_UInt64Data.Reset();
	m_Int64Data.Reset();
}

CBaseStatsSaveStructure::~CBaseStatsSaveStructure()
{
	m_IntData.Reset();   //	Do I need to call Reset() or is the Kill() in ~atArray() enough?
	m_FloatData.Reset();
	m_BoolData.Reset();
	m_UInt8Data.Reset();
	m_UInt16Data.Reset();
	m_UInt32Data.Reset();
	m_UInt64Data.Reset();
	m_Int64Data.Reset();
}


void CBaseStatsSaveStructure::PreSaveBaseStats(const bool bMultiplayerSave, const unsigned saveCategory)
{
	CStatsDataMgr &StatsDataMgr = CStatsMgr::GetStatsDataMgr();

	if (StatsDataMgr.GetCount() > 0)
	{
		statDebugf3("-----------------------------------------------------------------");
		statDebugf3("Save Stats");
		statDebugf3("-----------------------------------------------------------------");

		CStatsMgr::PreSaveBaseStats( bMultiplayerSave );

		u32 total_int_stats = 0;
		u32 total_float_stats = 0;
		u32 total_bool_stats = 0;
		u32 total_string_stats = 0;
		u32 total_uint8_stats = 0;
		u32 total_uint16_stats = 0;
		u32 total_uint32_stats = 0;
		u32 total_uint64_stats = 0;
		u32 total_int64_stats = 0;

		u32 number_of_int_stats = 0;
		u32 number_of_float_stats = 0;
		u32 number_of_bool_stats = 0;
		u32 number_of_string_stats = 0;
		u32 number_of_uint8_stats = 0;
		u32 number_of_uint16_stats = 0;
		u32 number_of_uint32_stats = 0;
		u32 number_of_uint64_stats = 0;
		u32 number_of_int64_stats = 0;

		// First pass to calculate the required size of each array
		// Second pass to Reserve the correct size of array and fill in the elements
		for (u32 pass = 0; pass < 2; pass++)
		{
			if (pass == 1)
			{
				if (total_int_stats > 0)    { m_IntData.Reset(); m_IntData.Resize(total_int_stats); }
				if (total_float_stats > 0)  { m_FloatData.Reset(); m_FloatData.Resize(total_float_stats); }
				if (total_bool_stats > 0)   { m_BoolData.Reset(); m_BoolData.Resize(total_bool_stats); }
				if (total_string_stats > 0) { m_StringData.Reset(); m_StringData.Resize(total_string_stats); }
				if (total_uint8_stats > 0)  { m_UInt8Data.Reset(); m_UInt8Data.Resize(total_uint8_stats); }
				if (total_uint16_stats > 0) { m_UInt16Data.Reset(); m_UInt16Data.Resize(total_uint16_stats); }
				if (total_uint32_stats > 0) { m_UInt32Data.Reset(); m_UInt32Data.Resize(total_uint32_stats); }
				if (total_uint64_stats > 0) { m_UInt64Data.Reset(); m_UInt64Data.Resize(total_uint64_stats); }
				if (total_int64_stats > 0)  { m_Int64Data.Reset(); m_Int64Data.Resize(total_int64_stats); }
			}

			CStatsDataMgr::StatsMap::Iterator iter = StatsDataMgr.StatIterBegin();
			while (iter != StatsDataMgr.StatIterEnd())
			{
				const sStatData* pStat = *iter;

				//Online stats are not saved in the save game...
				if (statVerify(pStat) && pStat->GetDesc().GetIsSavedInSaveGame(bMultiplayerSave, saveCategory))
				{
					const StatType type = pStat->GetType();
					switch(type)
					{
					case STAT_TYPE_TEXTLABEL:
					case STAT_TYPE_INT:
						{
							if (pass == 0)
							{
								total_int_stats++;
							}
							else
							{
								m_IntData[number_of_int_stats].m_NameHash = iter.GetKey();
								m_IntData[number_of_int_stats].m_Data     = pStat->GetInt();

								statDebugf3("... \"%d\" -\"%d\"", m_IntData[number_of_int_stats].m_NameHash.GetHash(), m_IntData[number_of_int_stats].m_Data);

								DURANGO_ONLY( events_durango::OnPreSavegame(iter.GetKey(), &m_IntData[number_of_int_stats].m_Data, pStat->GetSizeOfData()); )

									number_of_int_stats++;
							}
						}
						break;

					case STAT_TYPE_STRING:
						{
							if (pass == 0)
							{
								total_string_stats++;
							}
							else
							{
								m_StringData[number_of_string_stats].m_NameHash = iter.GetKey();
								m_StringData[number_of_string_stats].m_Data     = pStat->GetString();

								statDebugf3("... \"%d\" -\"%s\"", m_StringData[number_of_string_stats].m_NameHash.GetHash(), m_StringData[number_of_string_stats].m_Data.c_str());

								DURANGO_ONLY( events_durango::OnPreSavegame(iter.GetKey(), &m_StringData[number_of_string_stats].m_Data, pStat->GetSizeOfData()); )

									number_of_string_stats++;
							}
						}
						break;
					case STAT_TYPE_FLOAT:
						{
							if (pass == 0)
							{
								total_float_stats++;
							}
							else
							{
								m_FloatData[number_of_float_stats].m_NameHash = iter.GetKey();
								m_FloatData[number_of_float_stats].m_Data     = pStat->GetFloat();

								statDebugf3("... \"%d\" -\"%f\"", m_FloatData[number_of_float_stats].m_NameHash.GetHash(), m_FloatData[number_of_float_stats].m_Data);

								DURANGO_ONLY( events_durango::OnPreSavegame(iter.GetKey(), &m_FloatData[number_of_float_stats].m_Data, pStat->GetSizeOfData()); )

									number_of_float_stats++;
							}
						}
						break;
					case STAT_TYPE_BOOLEAN:
						{
							if (pass == 0)
							{
								total_bool_stats++;
							}
							else
							{
								m_BoolData[number_of_bool_stats].m_NameHash = iter.GetKey();
								m_BoolData[number_of_bool_stats].m_Data     = pStat->GetBoolean();

								statDebugf3("... \"%d\" -\"%s\"", m_BoolData[number_of_bool_stats].m_NameHash.GetHash(), m_BoolData[number_of_bool_stats].m_Data ? "TRUE" : "FALSE");

								DURANGO_ONLY( events_durango::OnPreSavegame(iter.GetKey(), &m_BoolData[number_of_bool_stats].m_Data, pStat->GetSizeOfData()); )

									number_of_bool_stats++;
							}
						}
						break;
					case STAT_TYPE_UINT8:
						{
							if (pass == 0)
							{
								total_uint8_stats++;
							}
							else
							{
								m_UInt8Data[number_of_uint8_stats].m_NameHash = iter.GetKey();
								m_UInt8Data[number_of_uint8_stats].m_Data     = pStat->GetUInt8();

								statDebugf3("... \"%d\" -\"%u\"", m_UInt8Data[number_of_uint8_stats].m_NameHash.GetHash(), m_UInt8Data[number_of_uint8_stats].m_Data);

								DURANGO_ONLY( events_durango::OnPreSavegame(iter.GetKey(), &m_UInt8Data[number_of_uint8_stats].m_Data, pStat->GetSizeOfData()); )

									number_of_uint8_stats++;
							}
						}
						break;
					case STAT_TYPE_UINT16:
						{
							if (pass == 0)
							{
								total_uint16_stats++;
							}
							else
							{
								m_UInt16Data[number_of_uint16_stats].m_NameHash = iter.GetKey();
								m_UInt16Data[number_of_uint16_stats].m_Data     = pStat->GetUInt16();

								statDebugf3("... \"%d\" -\"%u\"", m_UInt16Data[number_of_uint16_stats].m_NameHash.GetHash(), m_UInt16Data[number_of_uint16_stats].m_Data);

								DURANGO_ONLY( events_durango::OnPreSavegame(iter.GetKey(), &m_UInt16Data[number_of_uint16_stats].m_Data, pStat->GetSizeOfData()); )

									number_of_uint16_stats++;
							}
						}
						break;
					case STAT_TYPE_UINT32:
						{
							if (pass == 0)
							{
								total_uint32_stats++;
							}
							else
							{
								m_UInt32Data[number_of_uint32_stats].m_NameHash = iter.GetKey();
								m_UInt32Data[number_of_uint32_stats].m_Data     = pStat->GetUInt32();

								statDebugf3("... \"%d\" -\"%u\"", m_UInt32Data[number_of_uint32_stats].m_NameHash.GetHash(), m_UInt32Data[number_of_uint32_stats].m_Data);

								DURANGO_ONLY( events_durango::OnPreSavegame(iter.GetKey(), &m_UInt32Data[number_of_uint32_stats].m_Data, pStat->GetSizeOfData()); )

									number_of_uint32_stats++;
							}
						}
						break;
					case STAT_TYPE_UINT64:
					case STAT_TYPE_DATE:
					case STAT_TYPE_POS:
					case STAT_TYPE_PACKED:
					case STAT_TYPE_USERID:
						{
							if (pass == 0)
							{
								total_uint64_stats++;
							}
							else
							{
								m_UInt64Data[number_of_uint64_stats].m_NameHash = iter.GetKey();

								u64 data64 = pStat->GetUInt64();
								m_UInt64Data[number_of_uint64_stats].m_DataLow  = data64 & 0xFFFFFFFF;
								m_UInt64Data[number_of_uint64_stats].m_DataHigh = data64 >> 32;

								statDebugf3("... \"%d\" -\"%" I64FMT "d\"", m_UInt64Data[number_of_uint64_stats].m_NameHash.GetHash(), data64);

								DURANGO_ONLY( events_durango::OnPreSavegame(iter.GetKey(), &data64, pStat->GetSizeOfData()); )

									number_of_uint64_stats++;
							}
						}
						break;

					case STAT_TYPE_INT64:
						{
							if (pass == 0)
							{
								total_int64_stats++;
							}
							else
							{
								m_Int64Data[number_of_int64_stats].m_NameHash = iter.GetKey();

								s64 data64 = pStat->GetInt64();
								m_Int64Data[number_of_int64_stats].m_DataLow  = data64 & 0xFFFFFFFF;
								m_Int64Data[number_of_int64_stats].m_DataHigh = data64 >> 32;

								statDebugf3("... \"%d\" -\"%" I64FMT "d\"", m_Int64Data[number_of_int64_stats].m_NameHash.GetHash(), data64);

								DURANGO_ONLY( events_durango::OnPreSavegame(iter.GetKey(), &data64, pStat->GetSizeOfData()); )

									number_of_int64_stats++;
							}
						}
						break;

					default:
						savegameAssertf(0, "Save Stats - NOTYPE - \"%s\"", iter.GetKeyName());
					}
				}

				iter++;
			}
		}	//	for (u32 pass = 0; pass < 2; pass++)

		statDebugf3("-----------------------------------------------------------------");
	}

	savegameDisplayf("Number of elements in m_IntData is %d", m_IntData.GetCount());
	savegameDisplayf("Number of elements in m_FloatData is %d", m_FloatData.GetCount());
	savegameDisplayf("Number of elements in m_BoolData is %d", m_BoolData.GetCount());
	savegameDisplayf("Number of elements in m_UInt8Data is %d", m_UInt8Data.GetCount());
	savegameDisplayf("Number of elements in m_UInt16Data is %d", m_UInt16Data.GetCount());
	savegameDisplayf("Number of elements in m_UInt32Data is %d", m_UInt32Data.GetCount());
	savegameDisplayf("Number of elements in m_UInt64Data is %d", m_UInt64Data.GetCount());
	savegameDisplayf("Number of elements in m_Int64Data is %d", m_Int64Data.GetCount());
}

//	Should I use bMultiplayerSave to verify that the stats that are read are MP stats?
void CBaseStatsSaveStructure::PostLoadBaseStats(const bool bMultiplayerSave)
{
	CStatsDataMgr &StatsDataMgr = CStatsMgr::GetStatsDataMgr();
	int loop = 0;
	const u32 setDataFlags = bMultiplayerSave ? STATUPDATEFLAG_LOAD_FROM_MP_SAVEGAME : STATUPDATEFLAG_LOAD_FROM_SP_SAVEGAME;

	if (StatsDataMgr.GetCount() > 0)
	{
		statDebugf3("-----------------------------------------------------------------");
		statDebugf3("Load Stats");
		statDebugf3("-----------------------------------------------------------------");

		for (loop = 0; loop < m_IntData.GetCount(); loop++)
		{
			StatId keyHash = STAT_ID(m_IntData[loop].m_NameHash.GetHash());
			CStatsDataMgr::StatsMap::Iterator statIter;

			if (StatsDataMgr.StatIterFind(keyHash, statIter) && statIter.IsValid() && statIter.GetKey().IsValid())
			{
				const sStatData* pStat = *statIter;
				if (savegameVerify(pStat))
				{
					if (pStat->GetIsBaseType(STAT_TYPE_INT) || pStat->GetIsBaseType(STAT_TYPE_TEXTLABEL))
					{
						StatsDataMgr.SetStatIterData(statIter, &m_IntData[loop].m_Data, sizeof(m_IntData[loop].m_Data), setDataFlags);
						statDebugf3(".... %s  - \"%s\" -\"%d\"", pStat->GetIsBaseType(STAT_TYPE_INT) ? "Int" : "Label", statIter.GetKeyName(), m_IntData[loop].m_Data);
					}
					else
					{
						statWarningf("CBaseStatsSaveStructure::PostLoadBaseStats - stat with hash %d is not an int or label stat", m_IntData[loop].m_NameHash.GetHash());
					}
				}
			}
			else
			{
				statWarningf("CBaseStatsSaveStructure::PostLoadBaseStats - int/label stat with hash %d is not valid", m_IntData[loop].m_NameHash.GetHash());
			}
		}

		for (loop = 0; loop < m_StringData.GetCount(); loop++)
		{
			StatId keyHash = STAT_ID(m_StringData[loop].m_NameHash.GetHash());

			CStatsDataMgr::StatsMap::Iterator statIter;

			if (StatsDataMgr.StatIterFind(keyHash, statIter) && statIter.IsValid() && statIter.GetKey().IsValid())
			{
				const sStatData* pStat = *statIter;
				if (savegameVerify(pStat))
				{
					if (pStat->GetIsBaseType(STAT_TYPE_STRING))
					{
						StatsDataMgr.SetStatIterData(statIter, (void*)m_StringData[loop].m_Data.c_str(), m_StringData[loop].m_Data.length(), setDataFlags);
						statDebugf3(".... String  - \"%s\" -\"%s\"", statIter.GetKeyName(), m_StringData[loop].m_Data.c_str());
					}
					else
					{
						statWarningf("CBaseStatsSaveStructure::PostLoadBaseStats - stat with hash %d is not a string stat", m_StringData[loop].m_NameHash.GetHash());
					}
				}
			}
			else
			{
				statWarningf("CBaseStatsSaveStructure::PostLoadBaseStats - string stat with hash %d is not valid", m_StringData[loop].m_NameHash.GetHash());
			}
		}

		for (loop = 0; loop < m_FloatData.GetCount(); loop++)
		{
			StatId keyHash = STAT_ID(m_FloatData[loop].m_NameHash.GetHash());

			CStatsDataMgr::StatsMap::Iterator statIter;
			if (StatsDataMgr.StatIterFind(keyHash, statIter) && statIter.IsValid() && statIter.GetKey().IsValid())
			{
				const sStatData* pStat = *statIter;
				if (savegameVerify(pStat))
				{
					if (pStat->GetIsBaseType(STAT_TYPE_FLOAT))
					{
						StatsDataMgr.SetStatIterData(statIter, &m_FloatData[loop].m_Data, sizeof(m_FloatData[loop].m_Data), setDataFlags);
						statDebugf3(".... FLOAT  - \"%s\" -\"%f\"", statIter.GetKeyName(), m_FloatData[loop].m_Data);
					}
					else
					{
						statWarningf("CBaseStatsSaveStructure::PostLoadBaseStats - stat with hash %d is not a float stat", m_FloatData[loop].m_NameHash.GetHash());
					}
				}
			}
			else
			{
				statWarningf("CBaseStatsSaveStructure::PostLoadBaseStats - float stat with hash %d is not valid", m_FloatData[loop].m_NameHash.GetHash());
			}
		}

		for (loop = 0; loop < m_BoolData.GetCount(); loop++)
		{
			StatId keyHash = STAT_ID(m_BoolData[loop].m_NameHash.GetHash());

			CStatsDataMgr::StatsMap::Iterator statIter;
			if (StatsDataMgr.StatIterFind(keyHash, statIter) && statIter.IsValid() && statIter.GetKey().IsValid())
			{
				const sStatData* pStat = *statIter;
				if (savegameVerify(pStat))
				{
					if (pStat->GetIsBaseType(STAT_TYPE_BOOLEAN))
					{
						StatsDataMgr.SetStatIterData(statIter, &m_BoolData[loop].m_Data, sizeof(m_BoolData[loop].m_Data), setDataFlags);
						statDebugf3(".... BOOLEAN  - \"%s\" -\"%s\"", statIter.GetKeyName(), m_BoolData[loop].m_Data ? "TRUE" : "FALSE");
					}
					else
					{
						statWarningf("CBaseStatsSaveStructure::PostLoadBaseStats - stat with hash %d is not a bool stat", m_BoolData[loop].m_NameHash.GetHash());
					}
				}
			}
			else
			{
				statWarningf("CBaseStatsSaveStructure::PostLoadBaseStats - bool stat with hash %d is not valid", m_BoolData[loop].m_NameHash.GetHash());
			}
		}

		for (loop = 0; loop < m_UInt8Data.GetCount(); loop++)
		{
			StatId keyHash = STAT_ID(m_UInt8Data[loop].m_NameHash.GetHash());

			CStatsDataMgr::StatsMap::Iterator statIter;
			if (StatsDataMgr.StatIterFind(keyHash, statIter) && statIter.IsValid() && statIter.GetKey().IsValid())
			{
				const sStatData* pStat = *statIter;
				if (savegameVerify(pStat))
				{
					if (pStat->GetIsBaseType(STAT_TYPE_UINT8))
					{
						StatsDataMgr.SetStatIterData(statIter, &m_UInt8Data[loop].m_Data, sizeof(m_UInt8Data[loop].m_Data), setDataFlags);
						statDebugf3(".... UINT8  - \"%s\" -\"%u\"", statIter.GetKeyName(), m_UInt8Data[loop].m_Data);
					}
					else
					{
						statWarningf("CBaseStatsSaveStructure::PostLoadBaseStats - stat with hash %d is not a u8 stat", m_UInt8Data[loop].m_NameHash.GetHash());
					}
				}
			}
			else
			{
				statWarningf("CBaseStatsSaveStructure::PostLoadBaseStats - u8 stat with hash %d is not valid", m_UInt8Data[loop].m_NameHash.GetHash());
			}
		}

		for (loop = 0; loop < m_UInt16Data.GetCount(); loop++)
		{
			StatId keyHash = STAT_ID(m_UInt16Data[loop].m_NameHash.GetHash());

			CStatsDataMgr::StatsMap::Iterator statIter;
			if (StatsDataMgr.StatIterFind(keyHash, statIter) && statIter.IsValid() && statIter.GetKey().IsValid())
			{
				const sStatData* pStat = *statIter;
				if (savegameVerify(pStat))
				{
					if (pStat->GetIsBaseType(STAT_TYPE_UINT16))
					{
						StatsDataMgr.SetStatIterData(statIter, &m_UInt16Data[loop].m_Data, sizeof(m_UInt16Data[loop].m_Data), setDataFlags);
						statDebugf3(".... UINT16  - \"%s\" -\"%u\"", statIter.GetKeyName(), m_UInt16Data[loop].m_Data);
					}
					else
					{
						statWarningf("CBaseStatsSaveStructure::PostLoadBaseStats - stat with hash %d is not a u16 stat", m_UInt16Data[loop].m_NameHash.GetHash());
					}
				}
			}
			else
			{
				statWarningf("CBaseStatsSaveStructure::PostLoadBaseStats - u16 stat with hash %d is not valid", m_UInt16Data[loop].m_NameHash.GetHash());
			}
		}

		for (loop = 0; loop < m_UInt32Data.GetCount(); loop++)
		{
			StatId keyHash = STAT_ID(m_UInt32Data[loop].m_NameHash.GetHash());

			CStatsDataMgr::StatsMap::Iterator statIter;
			if (StatsDataMgr.StatIterFind(keyHash, statIter) && statIter.IsValid() && statIter.GetKey().IsValid())
			{
				const sStatData* pStat = *statIter;
				if (savegameVerify(pStat))
				{
					if (pStat->GetIsBaseType(STAT_TYPE_UINT32))
					{
						StatsDataMgr.SetStatIterData(statIter, &m_UInt32Data[loop].m_Data, sizeof(m_UInt32Data[loop].m_Data), setDataFlags);
						statDebugf3(".... UINT32  - \"%s\" -\"%u\"", statIter.GetKeyName(), m_UInt32Data[loop].m_Data);
					}
					else if(pStat->GetIsBaseType(STAT_TYPE_UINT64))
					{
						u64 newValue = (u64)m_UInt32Data[loop].m_Data;
						StatsDataMgr.SetStatIterData(statIter, &newValue, sizeof(u64), setDataFlags);
						statDebugf3(".... UINT64  - \"%s\" -\"%" I64FMT "d\"", statIter.GetKeyName(), newValue);
					}
					else
					{
						statWarningf("CBaseStatsSaveStructure::PostLoadBaseStats - stat with hash %d is not a u32 stat", m_UInt32Data[loop].m_NameHash.GetHash());
					}
				}
			}
			else
			{
				statWarningf("CBaseStatsSaveStructure::PostLoadBaseStats - u32 stat with hash %d is not valid", m_UInt32Data[loop].m_NameHash.GetHash());
			}
		}

		//	Dates are also stored in the u64 array
		for (loop = 0; loop < m_UInt64Data.GetCount(); loop++)
		{
			StatId keyHash = STAT_ID(m_UInt64Data[loop].m_NameHash.GetHash());

			CStatsDataMgr::StatsMap::Iterator statIter;
			if (StatsDataMgr.StatIterFind(keyHash, statIter) && statIter.IsValid() && statIter.GetKey().IsValid())
			{
				const sStatData* pStat = *statIter;
				if (savegameVerify(pStat))
				{
					if (pStat->GetIsBaseType(STAT_TYPE_UINT64)
						|| pStat->GetIsBaseType(STAT_TYPE_DATE)
						|| pStat->GetIsBaseType(STAT_TYPE_POS)
						|| pStat->GetIsBaseType(STAT_TYPE_PACKED)
						|| pStat->GetIsBaseType(STAT_TYPE_USERID))
					{
						u64 data64 = static_cast<u64>(m_UInt64Data[loop].m_DataHigh) << 32;
						data64 += static_cast<u64>(m_UInt64Data[loop].m_DataLow);

						StatsDataMgr.SetStatIterData(statIter, &data64, sizeof(data64), setDataFlags);
						statDebugf3("... \"%s\" -\"%" I64FMT "d\"", statIter.GetKeyName(), data64);
					}
					else
					{
						statWarningf("CBaseStatsSaveStructure::PostLoadBaseStats - stat with hash %d is not a u64 stat", m_UInt64Data[loop].m_NameHash.GetHash());
					}
				}
			}
			else
			{
				statWarningf("CBaseStatsSaveStructure::PostLoadBaseStats - u64 stat with hash %d is not valid", m_UInt64Data[loop].m_NameHash.GetHash());
			}
		}

		//	Dates are also stored in the s64 array
		for (loop = 0; loop < m_Int64Data.GetCount(); loop++)
		{
			StatId keyHash = STAT_ID(m_Int64Data[loop].m_NameHash.GetHash());

			CStatsDataMgr::StatsMap::Iterator statIter;
			if (StatsDataMgr.StatIterFind(keyHash, statIter) && statIter.IsValid() && statIter.GetKey().IsValid())
			{
				const sStatData* pStat = *statIter;
				if (savegameVerify(pStat))
				{
					if (pStat->GetIsBaseType(STAT_TYPE_INT64))
					{
						s64 data64 = static_cast<s64>(m_Int64Data[loop].m_DataHigh) << 32;
						data64 = data64 | static_cast<s64>(m_Int64Data[loop].m_DataLow);

						StatsDataMgr.SetStatIterData(statIter, &data64, sizeof(data64), setDataFlags);
						statDebugf3("... \"%s\" -\"%" I64FMT "d\"", statIter.GetKeyName(), data64);
					}
					else
					{
						statWarningf("CBaseStatsSaveStructure::PostLoadBaseStats - stat with hash %d is not a s64 stat", m_Int64Data[loop].m_NameHash.GetHash());
					}
				}
			}
			else
			{
				statWarningf("CBaseStatsSaveStructure::PostLoadBaseStats - s64 stat with hash %d is not valid", m_Int64Data[loop].m_NameHash.GetHash());
			}
		}

		StatsDataMgr.PostLoadBaseStats(bMultiplayerSave);

		statDebugf3("-----------------------------------------------------------------");
	}
}

CStatsSaveStructure::CStatsSaveStructure()
{
	m_PedsKilledOfThisType.Reset();
	m_aStationPlayTime.Reset();
	m_SpVehiclesDriven.Reset();
	m_currCountAsFacebookDriven = 0;
	m_vehicleRecords.Reset();
}

CStatsSaveStructure::~CStatsSaveStructure()
{
	m_PedsKilledOfThisType.Reset();
	m_aStationPlayTime.Reset();
	m_SpVehiclesDriven.Reset();
	m_currCountAsFacebookDriven = 0;
	m_vehicleRecords.Reset();
}

void CStatsSaveStructure::PreSave( )
{
	if (!CGenericGameStorage::AllowPreSaveAndPostLoad("CStatsSaveStructure::PreSave"))
	{
		return;
	}

	CBaseStatsSaveStructure::PreSaveBaseStats(false, MAX_STAT_SAVE_CATEGORY);

	int loop = 0;

	atHashString HashStringToUseAsKey;
	m_PedsKilledOfThisType.Reset();
	for (loop = 0; loop < PEDTYPE_LAST_PEDTYPE; loop++)
	{
		if (savegameVerifyf(CSaveGameMappingEnumsToHashStrings::GetHashString_PedType(loop, HashStringToUseAsKey), "CStatsSaveStructure::PreSave - failed to find a hash string for ped type %d", loop))
		{
			m_PedsKilledOfThisType.Insert(HashStringToUseAsKey, CStatsMgr::sm_PedsKilledOfThisType[loop]);
		}
	}
	m_PedsKilledOfThisType.FinishInsertion();

	//Track Single Player Models Driven.
	m_SpVehiclesDriven.Reset();
	for (u32 i=0; i<CStatsMgr::sm_SpVehicleDriven.GetCount(); i++)
		m_SpVehiclesDriven.PushAndGrow(CStatsMgr::sm_SpVehicleDriven[i]);

	//Number of vehicles already driven.
	m_currCountAsFacebookDriven = CStatsMgr::sm_currCountAsFacebookDriven;

	//Vehicle Records
	m_vehicleRecords.Resize(CStatsMgr::sm_vehicleRecords.GetCount());
	for (int i=0; i<CStatsMgr::sm_vehicleRecords.GetCount(); i++)
		m_vehicleRecords[i] = CStatsMgr::sm_vehicleRecords[i];

#if NA_RADIO_ENABLED
	// 	m_aStationPlayTime.Resize(19);
	// 	for (loop = 0; loop < 19/*MAX_NUM_STATIOS*/; loop++)
	// 	{
	// 		m_aStationPlayTime[loop] = 0.0f;//CRadioHudDataManagement::sm_aStationPlayTime[loop];
	// 	}
	m_aStationPlayTime.Reset();
#endif
}


void CStatsSaveStructure::PostLoad()
{
	if (!CGenericGameStorage::AllowPreSaveAndPostLoad("CStatsSaveStructure::PostLoad"))
	{
		return;
	}

	CBaseStatsSaveStructure::PostLoadBaseStats(false);

	SStuntJumpManager::GetInstance().ValidateStuntJumpStats(true);

	//Track different Ped types killed by the player
	atBinaryMap<int, atHashString>::Iterator PedsKilledIterator = m_PedsKilledOfThisType.Begin();
	while (PedsKilledIterator != m_PedsKilledOfThisType.End())
	{
		const atHashString PedTypeHashString = PedsKilledIterator.GetKey();
		s32 PedTypeIndex = CSaveGameMappingEnumsToHashStrings::FindIndexFromHashString_PedType(PedTypeHashString);
		if (savegameVerifyf(PedTypeIndex >= 0, "CStatsSaveStructure::PostLoad - couldn't find ped type index for hash string %s", PedTypeHashString.GetCStr()))
		{
			CStatsMgr::sm_PedsKilledOfThisType[PedTypeIndex] = *PedsKilledIterator;			
		}

		++PedsKilledIterator;
	}

	//Track Single Player Models Driven
	CStatsMgr::sm_SpVehicleDriven.Reset();
	for (u32 i=0; i<m_SpVehiclesDriven.GetCount(); i++)
	{
		if (!CStatsMgr::VehicleHasBeenDriven(m_SpVehiclesDriven[i], false))
		{
			CStatsMgr::sm_SpVehicleDriven.PushAndGrow(m_SpVehiclesDriven[i]);
		}
	}

	//number of vehicles count driven
	CStatsMgr::sm_currCountAsFacebookDriven = m_currCountAsFacebookDriven;

	//Vehicle Records
	for (int i=0; i<CStatsMgr::sm_vehicleRecords.GetCount(); i++)
		CStatsMgr::sm_vehicleRecords[i].Clear(true);
	CStatsMgr::sm_vehicleRecords.Resize(m_vehicleRecords.GetCount());
	for (int i=0; i<m_vehicleRecords.GetCount(); i++)
		CStatsMgr::sm_vehicleRecords[i] = m_vehicleRecords[i];

	CStatsMgr::sm_curVehIdx = CStatsMgr::INVALID_RECORD;

	//Load is finished - synch all profile stats.
	//Updated this to force a sync
	//Note: CStatsSaveStructure is for SP, we sync profile stats in response
	//to loading an MP savegame in CStatsSavesMgr::Update()
	if (rlProfileStats::IsInitialized() && !CLiveManager::GetProfileStatsMgr().SynchronizePending(CProfileStats::PS_SYNC_SP))
	{
		StatsInterface::SyncProfileStats(true/*force*/, false/*multiplayer*/);
	}

	CPed* pPlayerPed = CGameWorld::FindLocalPlayer();

	if (savegameVerifyf(pPlayerPed, "CStatsSaveStructure::PostLoad - failed to find the local player ped")
		&& savegameVerifyf(pPlayerPed->GetPlayerInfo(), "CStatsSaveStructure::PostLoad - local player ped has no player info"))
	{
		CPlayerSpecialAbility* specialAbility = pPlayerPed->GetSpecialAbility();
		if (savegameVerifyf(specialAbility, "CStatsSaveStructure::PostLoad - player has no special ability"))
		{
			specialAbility->UpdateFromStat();
		}
	}

	CStatsMgr::WriteAfterGameLoad( );
}



CMultiplayerStatsSaveStructure::CMultiplayerStatsSaveStructure()
{
	m_MpVehiclesDriven.Reset();
}

CMultiplayerStatsSaveStructure::~CMultiplayerStatsSaveStructure()
{
	m_MpVehiclesDriven.Reset();
}

void CMultiplayerStatsSaveStructure::PreSave()
{
	if (!CGenericGameStorage::AllowPreSaveAndPostLoad("CMultiplayerStatsSaveStructure::PreSave"))
	{
		savegameAssertf(0, "CMultiplayerStatsSaveStructure::PreSave - didn't expect this MP function to get called during the import/export of an SP savegame");
		return;
	}

	CBaseStatsSaveStructure::PreSaveBaseStats(true, CGenericGameStorage::GetStatsSaveCategory());

	//Remaining stats are saved in the DEFAULT savegame file only.
	if (STAT_MP_CATEGORY_DEFAULT == CGenericGameStorage::GetStatsSaveCategory())
	{
		//Track Multiplayer Player Models Driven
		m_MpVehiclesDriven.Reset();
		for (u32 i=0; i<CStatsMgr::sm_MpVehicleDriven.GetCount(); i++)
		{
			m_MpVehiclesDriven.PushAndGrow(CStatsMgr::sm_MpVehicleDriven[i]);
		}
	}
}


void CMultiplayerStatsSaveStructure::PostLoad()
{
	if (!CGenericGameStorage::AllowPreSaveAndPostLoad("CMultiplayerStatsSaveStructure::PostLoad"))
	{
		savegameAssertf(0, "CMultiplayerStatsSaveStructure::PostLoad - didn't expect this MP function to get called during the import/export of an SP savegame");
		return;
	}

	//Check timestamp for Multi Player
	const char* statName = "_SaveMpTimestamp_0";
	switch (CGenericGameStorage::GetStatsSaveCategory())
	{
	case STAT_MP_CATEGORY_DEFAULT: statName = "_SaveMpTimestamp_0"; break;
	case STAT_MP_CATEGORY_CHAR0:   statName = "_SaveMpTimestamp_1"; break;
	case STAT_MP_CATEGORY_CHAR1:   statName = "_SaveMpTimestamp_2"; break;
	}
	StatId statTimestamp(statName);

	u64 localTimeStamp = 0;
	u64 data64 = 0;

	StatsInterface::GetStatData(statTimestamp, &localTimeStamp, sizeof(u64));

	bool found = false;
	for (int loop = 0; loop < m_UInt64Data.GetCount() && !found; loop++)
	{
		if (statTimestamp.GetHash() == m_UInt64Data[loop].m_NameHash.GetHash())
		{
			found = true;

			data64 = static_cast<u64>(m_UInt64Data[loop].m_DataHigh) << 32;
			data64 += static_cast<u64>(m_UInt64Data[loop].m_DataLow);
		}
	}

	if (localTimeStamp > data64)
	{
		savegameWarningf("CBaseStatsSaveStructure::PostLoadBaseStats - MULTIPLAYER Dont load local timestamp is bigger");
		return;
	}

	CBaseStatsSaveStructure::PostLoadBaseStats(true);

	//Track Multiplayer Player Models Driven
	if (0 < m_MpVehiclesDriven.GetCount())
	{
		CStatsMgr::sm_MpVehicleDriven.Reset();
		for (u32 i=0; i<m_MpVehiclesDriven.GetCount(); i++)
		{
			if (!CStatsMgr::VehicleHasBeenDriven(m_MpVehiclesDriven[i], true))
			{
				CStatsMgr::sm_MpVehicleDriven.PushAndGrow(m_MpVehiclesDriven[i]);
			}
		}
	}
}




#if __ALLOW_IMPORT_OF_SP_SAVEGAMES
void CStatsSaveStructure::Initialize(CStatsSaveStructure_Migration &copyFrom)
{
	// Copy the nine arrays in CBaseStatsSaveStructure
	m_IntData = copyFrom.m_IntData;
	m_FloatData = copyFrom.m_FloatData;
	m_BoolData = copyFrom.m_BoolData;
	m_StringData = copyFrom.m_StringData;
	m_UInt8Data = copyFrom.m_UInt8Data;
	m_UInt16Data = copyFrom.m_UInt16Data;
	m_UInt32Data = copyFrom.m_UInt32Data;
	m_UInt64Data = copyFrom.m_UInt64Data;
	m_Int64Data = copyFrom.m_Int64Data;

	// Now copy the three members of CStatsSaveStructure that I don't need to change
	m_SpVehiclesDriven = copyFrom.m_SpVehiclesDriven;
	m_currCountAsFacebookDriven = copyFrom.m_currCountAsFacebookDriven;
	m_vehicleRecords = copyFrom.m_vehicleRecords;

	// Finally copy the contents of the two Binary Maps
	// The source binary maps use u32 for the key.
	// The binary maps in this class use atHashString for the key.
	// Hopefully the game already knows what the strings are for these atHashStrings.
	m_PedsKilledOfThisType.Reset();
	m_PedsKilledOfThisType.Reserve(copyFrom.m_PedsKilledOfThisType.GetCount());

	atBinaryMap<int, u32>::Iterator pedsKilledIterator = copyFrom.m_PedsKilledOfThisType.Begin();
	while (pedsKilledIterator != copyFrom.m_PedsKilledOfThisType.End())
	{
		u32 keyAsU32 = pedsKilledIterator.GetKey();
		atHashString keyAsHashString(keyAsU32);
		int valueToInsert = *pedsKilledIterator;
		m_PedsKilledOfThisType.Insert(keyAsHashString, valueToInsert);

		pedsKilledIterator++;
	}
	m_PedsKilledOfThisType.FinishInsertion();



	m_aStationPlayTime.Reset();
	m_aStationPlayTime.Reserve(copyFrom.m_aStationPlayTime.GetCount());

	atBinaryMap<float, u32>::Iterator stationsIterator = copyFrom.m_aStationPlayTime.Begin();
	while (stationsIterator != copyFrom.m_aStationPlayTime.End())
	{
		u32 keyAsU32 = stationsIterator.GetKey();
		atHashString keyAsHashString(keyAsU32);
		float valueToInsert = *stationsIterator;
		m_aStationPlayTime.Insert(keyAsHashString, valueToInsert);
		stationsIterator++;
	}
	m_aStationPlayTime.FinishInsertion();
}
#endif // __ALLOW_IMPORT_OF_SP_SAVEGAMES


//	#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
CStatsSaveStructure_Migration::CStatsSaveStructure_Migration()
{
	m_PedsKilledOfThisType.Reset();
	m_aStationPlayTime.Reset();
	m_SpVehiclesDriven.Reset();
	m_currCountAsFacebookDriven = 0;
	m_vehicleRecords.Reset();
}

CStatsSaveStructure_Migration::~CStatsSaveStructure_Migration()
{
	m_PedsKilledOfThisType.Reset();
	m_aStationPlayTime.Reset();
	m_SpVehiclesDriven.Reset();
	m_currCountAsFacebookDriven = 0;
	m_vehicleRecords.Reset();
}


void CStatsSaveStructure_Migration::Initialize(CStatsSaveStructure &copyFrom)
{
// Copy the nine arrays in CBaseStatsSaveStructure
	m_IntData = copyFrom.m_IntData;
	m_FloatData = copyFrom.m_FloatData;
	m_BoolData = copyFrom.m_BoolData;
	m_StringData = copyFrom.m_StringData;
	m_UInt8Data = copyFrom.m_UInt8Data;
	m_UInt16Data = copyFrom.m_UInt16Data;
	m_UInt32Data = copyFrom.m_UInt32Data;
	m_UInt64Data = copyFrom.m_UInt64Data;
	m_Int64Data = copyFrom.m_Int64Data;

// Now copy the three members of CStatsSaveStructure that I don't need to change
	m_SpVehiclesDriven = copyFrom.m_SpVehiclesDriven;
	m_currCountAsFacebookDriven = copyFrom.m_currCountAsFacebookDriven;
	m_vehicleRecords = copyFrom.m_vehicleRecords;

// Finally copy the contents of the two Binary Maps
// The original binary maps used atHashString for the key.
// The string for an atHashString is stripped out of Final executables.
// The binary maps in this class use u32 for the key so that they can be written to an XML file by a Final executable.

	m_PedsKilledOfThisType.Reset();
	m_PedsKilledOfThisType.Reserve(copyFrom.m_PedsKilledOfThisType.GetCount());

	atBinaryMap<int, atHashString>::Iterator pedsKilledIterator = copyFrom.m_PedsKilledOfThisType.Begin();
	while (pedsKilledIterator != copyFrom.m_PedsKilledOfThisType.End())
	{
		u32 keyAsU32 = pedsKilledIterator.GetKey().GetHash();
		int valueToInsert = *pedsKilledIterator;
		m_PedsKilledOfThisType.Insert(keyAsU32, valueToInsert);

		pedsKilledIterator++;
	}
	m_PedsKilledOfThisType.FinishInsertion();



	m_aStationPlayTime.Reset();
	m_aStationPlayTime.Reserve(copyFrom.m_aStationPlayTime.GetCount());

	atBinaryMap<float, atHashString>::Iterator stationsIterator = copyFrom.m_aStationPlayTime.Begin();
	while (stationsIterator != copyFrom.m_aStationPlayTime.End())
	{
		u32 keyAsU32 = stationsIterator.GetKey().GetHash();
		float valueToInsert = *stationsIterator;
		m_aStationPlayTime.Insert(keyAsU32, valueToInsert);
		stationsIterator++;
	}
	m_aStationPlayTime.FinishInsertion();
}

void CStatsSaveStructure_Migration::PreSave( )
{
	if (!CGenericGameStorage::AllowPreSaveAndPostLoad("CStatsSaveStructure_Migration::PreSave"))
	{
		return;
	}
}

void CStatsSaveStructure_Migration::PostLoad()
{
	if (!CGenericGameStorage::AllowPreSaveAndPostLoad("CStatsSaveStructure_Migration::PostLoad"))
	{
		return;
	}
}


#if  __ALLOW_EXPORT_OF_SP_SAVEGAMES || __ALLOW_IMPORT_OF_SP_SAVEGAMES
s32 CStatsSaveStructure_Migration::GetHashOfNameOfLastMissionPassed()
{
 	StatId statLastMissionName("SP_LAST_MISSION_NAME");
	u32 hashToFind = statLastMissionName.GetHash();

	for (s32 loop = 0; loop < m_IntData.GetCount(); loop++)
	{
		if (m_IntData[loop].m_NameHash.GetHash() == hashToFind)
		{
			savegameDisplayf("CStatsSaveStructure_Migration::GetHashOfNameOfLastMissionPassed - found SP_LAST_MISSION_NAME - returning the hash %d", m_IntData[loop].m_Data);
			return m_IntData[loop].m_Data;
		}
	}

	savegameErrorf("CStatsSaveStructure_Migration::GetHashOfNameOfLastMissionPassed - failed to find SP_LAST_MISSION_NAME - returning 0");
	savegameAssertf(0, "CStatsSaveStructure_Migration::GetHashOfNameOfLastMissionPassed - failed to find SP_LAST_MISSION_NAME - returning 0");

	return 0;
}

float CStatsSaveStructure_Migration::GetSinglePlayerCompletionPercentage()
{
	StatId statTotalProgressMade("TOTAL_PROGRESS_MADE");
	u32 hashToFind = statTotalProgressMade.GetHash();

	for (s32 loop = 0; loop < m_FloatData.GetCount(); loop++)
	{
		if (m_FloatData[loop].m_NameHash.GetHash() == hashToFind)
		{
			savegameDisplayf("CStatsSaveStructure_Migration::GetSinglePlayerCompletionPercentage - found TOTAL_PROGRESS_MADE - returning %.1f", m_FloatData[loop].m_Data);
			return m_FloatData[loop].m_Data;
		}
	}

	savegameErrorf("CStatsSaveStructure_Migration::GetSinglePlayerCompletionPercentage - failed to find TOTAL_PROGRESS_MADE - returning 0.0f");
	savegameAssertf(0, "CStatsSaveStructure_Migration::GetSinglePlayerCompletionPercentage - failed to find TOTAL_PROGRESS_MADE - returning 0.0f");

	return 0.0f;
}
#endif //  __ALLOW_EXPORT_OF_SP_SAVEGAMES || __ALLOW_IMPORT_OF_SP_SAVEGAMES


//	#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES

