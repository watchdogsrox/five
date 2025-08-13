
#include "savegame_data_radio_stations.h"
#include "savegame_data_radio_stations_parser.h"


// Game headers
#include "SaveLoad/GenericGameStorage.h"
#include "SaveLoad/savegame_channel.h"



#if NA_RADIO_ENABLED
void CRadioStationSaveStructure::CopyRadioHistoryArrayForSaving(atArray<u32> &aDest, const audRadioStationHistory *aSource, s32 SizeOfArrayToCopy)
{
	aDest.Resize(SizeOfArrayToCopy);
	for (s32 HistoryLoop = 0; HistoryLoop < SizeOfArrayToCopy; HistoryLoop++)
	{
		aDest[HistoryLoop] = (*aSource)[HistoryLoop];
	}
}

void CRadioStationSaveStructure::CopyRadioHistoryArrayForLoading(audRadioStationHistory *aDest, atArray<u32> &aSource)
{
	for (s32 HistoryLoop = 0; HistoryLoop < Min<s32>(audRadioStationHistory::kRadioHistoryLength, aSource.GetCount()); HistoryLoop++)
	{
		(*aDest)[HistoryLoop] = aSource[HistoryLoop];
	}
}
#endif

void CRadioStationSaveStructure::PreSave()
{
	if (!CGenericGameStorage::AllowPreSaveAndPostLoad("CRadioStationSaveStructure::PreSave"))
	{
		return;
	}

#if NA_RADIO_ENABLED

	// now special track lists; news weather and adverts
	Assign(m_NewsReportHistoryWriteIndex, static_cast<u32>(audRadioStation::sm_NewsHistory.GetWriteIndex()));
	CopyRadioHistoryArrayForSaving(m_NewsReportHistorySpace, &audRadioStation::sm_NewsHistory, audRadioStationHistory::kRadioHistoryLength);

	Assign(m_WeatherReportHistoryWriteIndex, static_cast<u32>(audRadioStation::sm_WeatherHistory.GetWriteIndex()));
	CopyRadioHistoryArrayForSaving(m_WeatherReportHistorySpace, &audRadioStation::sm_WeatherHistory, audRadioStationHistory::kRadioHistoryLength);

	Assign(m_GenericAdvertHistoryWriteIndex, static_cast<u32>(audRadioStation::sm_AdvertHistory.GetWriteIndex()));
	CopyRadioHistoryArrayForSaving(m_GenericAdvertHistorySpace, &audRadioStation::sm_AdvertHistory, audRadioStationHistory::kRadioHistoryLength);

	// now news state
	s32 SizeOfArrayToCopy = audRadioStation::sm_NewsStoryState.GetMaxCount();

	m_NewsStoryState.Resize(SizeOfArrayToCopy);
	for (s32 HistoryLoop = 0; HistoryLoop < SizeOfArrayToCopy; HistoryLoop++)
	{
		m_NewsStoryState[HistoryLoop] = audRadioStation::sm_NewsStoryState[HistoryLoop];
	}

	// then per station music + dj history
	for(u32 i = 0; i < audRadioStation::sm_NumRadioStations; i++)
	{
		audRadioStation *station = audRadioStation::GetStation(i);
		if (savegameVerifyf(station, "CRadioStationSaveStructure::PreSave - failed to get pointer to radio station"))
		{
			CRadioStationStruct OneRadioStation;

			sysMemSet(&OneRadioStation, 0, sizeof(OneRadioStation));

			strncpy(OneRadioStation.m_RadioStationName, station->GetName(), g_MaxRadioNameLength);

			const audRadioStationHistory *list = station->FindHistoryForCategory(RADIO_TRACK_CAT_IDENTS);
			if(list)
			{
				Assign(OneRadioStation.m_IdentsHistoryWriteIndex, static_cast<u32>(list->GetWriteIndex()));
				CopyRadioHistoryArrayForSaving(OneRadioStation.m_IdentsHistorySpace, list, audRadioStationHistory::kRadioHistoryLength);
			}

			list = station->FindHistoryForCategory(RADIO_TRACK_CAT_MUSIC);
			if(list)
			{
				Assign(OneRadioStation.m_MusicHistoryWriteIndex, static_cast<u32>(list->GetWriteIndex()));
				CopyRadioHistoryArrayForSaving(OneRadioStation.m_MusicHistorySpace, list, audRadioStationHistory::kRadioHistoryLength);
			}

			list = station->FindHistoryForCategory(RADIO_TRACK_CAT_DJSOLO);
			if(list)
			{
				Assign(OneRadioStation.m_DjSoloHistoryWriteIndex, static_cast<u32>(list->GetWriteIndex()));
				CopyRadioHistoryArrayForSaving(OneRadioStation.m_DjSoloHistorySpace, list, audRadioStationHistory::kRadioHistoryLength);
			}

			// dj speech
			u32 historySize = station->m_DjSpeechHistory.GetMaxCount();
			OneRadioStation.m_DjSpeechHistoryWriteIndex = station->m_DjSpeechHistoryWriteIndex;
			OneRadioStation.m_DjSpeechHistory.Resize(historySize);
			for(u32 index = 0; index < historySize; index++)
			{
				OneRadioStation.m_DjSpeechHistory[index] = station->m_DjSpeechHistory[index];
			}

			OneRadioStation.m_ListenTimer = station->GetListenTimer();

			m_RadioStation.PushAndGrow(OneRadioStation);
		}
	}
#endif // NA_RADIO_ENABLED
}


void CRadioStationSaveStructure::PostLoad()
{
	if (!CGenericGameStorage::AllowPreSaveAndPostLoad("CRadioStationSaveStructure::PostLoad"))
	{
		return;
	}

#if NA_RADIO_ENABLED

	// first special track lists; news weather and adverts
	audRadioStation::sm_NewsHistory.SetWriteIndex(static_cast<s32>(m_NewsReportHistoryWriteIndex));
	CopyRadioHistoryArrayForLoading(&audRadioStation::sm_NewsHistory, m_NewsReportHistorySpace);

	audRadioStation::sm_WeatherHistory.SetWriteIndex(static_cast<s32>(m_WeatherReportHistoryWriteIndex));
	CopyRadioHistoryArrayForLoading(&audRadioStation::sm_WeatherHistory, m_WeatherReportHistorySpace);

	audRadioStation::sm_AdvertHistory.SetWriteIndex(static_cast<s32>(m_GenericAdvertHistoryWriteIndex));
	CopyRadioHistoryArrayForLoading(&audRadioStation::sm_AdvertHistory, m_GenericAdvertHistorySpace);

	// now news state
	for (s32 HistoryLoop = 0; HistoryLoop < m_NewsStoryState.GetCount(); HistoryLoop++)
	{
		audRadioStation::sm_NewsStoryState[HistoryLoop] = m_NewsStoryState[HistoryLoop];
	}

	for (u32 StationLoop = 0; StationLoop < m_RadioStation.GetCount(); StationLoop++)
	{
		audRadioStation *station = audRadioStation::FindStation(m_RadioStation[StationLoop].m_RadioStationName);

		if (savegameVerifyf(station, "CRadioStationSaveStructure::PostLoad - couldn't find radio station with name %s", m_RadioStation[StationLoop].m_RadioStationName))
		{
			if(!station->UseRandomizedStrideSelection())
			{
				audRadioStationHistory *list = const_cast<audRadioStationHistory *>(station->FindHistoryForCategory(RADIO_TRACK_CAT_IDENTS));
				if(list)
				{
					list->SetWriteIndex(static_cast<s32>(m_RadioStation[StationLoop].m_IdentsHistoryWriteIndex));
					CopyRadioHistoryArrayForLoading(list, m_RadioStation[StationLoop].m_IdentsHistorySpace);
				}

				list = const_cast<audRadioStationHistory *>(station->FindHistoryForCategory(RADIO_TRACK_CAT_MUSIC));
				if(list)
				{
					list->SetWriteIndex(static_cast<s32>(m_RadioStation[StationLoop].m_MusicHistoryWriteIndex));
					CopyRadioHistoryArrayForLoading(list, m_RadioStation[StationLoop].m_MusicHistorySpace);
				}

				list = const_cast<audRadioStationHistory *>(station->FindHistoryForCategory(RADIO_TRACK_CAT_DJSOLO));
				if(list)
				{
					list->SetWriteIndex(static_cast<s32>(m_RadioStation[StationLoop].m_DjSoloHistoryWriteIndex));
					CopyRadioHistoryArrayForLoading(list, m_RadioStation[StationLoop].m_DjSoloHistorySpace);
				}
			}			

			// dj speech
			station->m_DjSpeechHistoryWriteIndex = m_RadioStation[StationLoop].m_DjSpeechHistoryWriteIndex;

			s32 sizeOfSpeechHistoryArrayInSavegame = m_RadioStation[StationLoop].m_DjSpeechHistory.GetCount();
			for(u32 index = 0; index < station->m_DjSpeechHistory.GetMaxCount(); index++)
			{
				if (index < sizeOfSpeechHistoryArrayInSavegame)
				{
					station->m_DjSpeechHistory[index] = m_RadioStation[StationLoop].m_DjSpeechHistory[index];
				}
				else
				{
					station->m_DjSpeechHistory[index] = 0;
				}
			}

			station->SetListenTimer(m_RadioStation[StationLoop].m_ListenTimer);

			station->PostLoad();
		}
	}
#endif // NA_RADIO_ENABLED
}


