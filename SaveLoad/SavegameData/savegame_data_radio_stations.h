#ifndef SAVEGAME_DATA_RADIO_STATIONS_H
#define SAVEGAME_DATA_RADIO_STATIONS_H

// Rage headers
#include "atl/array.h"
#include "parser/macros.h"

// Game headers
#include "audio/audiodefines.h"
#include "audio/radiostation.h"


class CRadioStationSaveStructure
{
private:
#if NA_RADIO_ENABLED
	void CopyRadioHistoryArrayForSaving(atArray<u32> &aDest, const audRadioStationHistory *aSource, s32 SizeOfArrayToCopy);
	void CopyRadioHistoryArrayForLoading(audRadioStationHistory *aDest, atArray<u32> &aSource);
#endif
public :

	class CRadioStationStruct
	{
	public:
		float m_ListenTimer;

		char m_RadioStationName[g_MaxRadioNameLength];

		u8 m_IdentsHistoryWriteIndex;
		atArray<u32> m_IdentsHistorySpace;

		u8 m_MusicHistoryWriteIndex;
		atArray<u32> m_MusicHistorySpace;

		u8 m_DjSoloHistoryWriteIndex;
		atArray<u32> m_DjSoloHistorySpace;

		u8 m_DjSpeechHistoryWriteIndex;
		atArray<u32> m_DjSpeechHistory;

		PAR_SIMPLE_PARSABLE
	};

	u8 m_NewsReportHistoryWriteIndex;
	atArray<u32> m_NewsReportHistorySpace;

	u8 m_WeatherReportHistoryWriteIndex;
	atArray<u32> m_WeatherReportHistorySpace;

	u8 m_GenericAdvertHistoryWriteIndex;
	atArray<u32> m_GenericAdvertHistorySpace;

	atArray<u8> m_NewsStoryState;

	atArray<CRadioStationStruct> m_RadioStation;

	void PreSave();
	void PostLoad();

	PAR_SIMPLE_PARSABLE
};

#endif // SAVEGAME_DATA_RADIO_STATIONS_H

