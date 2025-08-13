#ifndef SAVEGAME_DATA_STATS_H
#define SAVEGAME_DATA_STATS_H


// Rage headers
#include "parser/macros.h"


// Game headers
#include "SaveLoad/savegame_defines.h"
#include "stats/StatsMgr.h"


// Forward declarations
#if __ALLOW_IMPORT_OF_SP_SAVEGAMES
class CStatsSaveStructure_Migration;
#endif // __ALLOW_IMPORT_OF_SP_SAVEGAMES


class CBaseStatsSaveStructure
{
public :

	class CIntStatStruct
	{
	public:
		atStatNameValue m_NameHash;
		int m_Data;

		PAR_SIMPLE_PARSABLE
	};

	class CStringStatStruct
	{
	public:
		atStatNameValue m_NameHash;
		atString m_Data;

		PAR_SIMPLE_PARSABLE
	};

	class CFloatStatStruct
	{
	public:
		atStatNameValue m_NameHash;
		float m_Data;

		PAR_SIMPLE_PARSABLE
	};

	class CBoolStatStruct
	{
	public:
		atStatNameValue m_NameHash;
		bool m_Data;

		PAR_SIMPLE_PARSABLE
	};

	class CUInt8StatStruct
	{
	public:
		atStatNameValue m_NameHash;
		u8 m_Data;

		PAR_SIMPLE_PARSABLE
	};

	class CUInt16StatStruct
	{
	public:
		atStatNameValue m_NameHash;
		u16 m_Data;

		PAR_SIMPLE_PARSABLE
	};

	class CUInt32StatStruct
	{
	public:
		atStatNameValue m_NameHash;
		u32 m_Data;

		PAR_SIMPLE_PARSABLE
	};


	class CUInt64StatStruct
	{
	public:
		atStatNameValue m_NameHash;
		u32 m_DataHigh;
		u32 m_DataLow;

		PAR_SIMPLE_PARSABLE
	};

	class CInt64StatStruct
	{
	public:
		atStatNameValue m_NameHash;
		u32 m_DataHigh;
		u32 m_DataLow;

		PAR_SIMPLE_PARSABLE
	};

	atArray<CIntStatStruct> m_IntData;
	atArray<CFloatStatStruct> m_FloatData;
	atArray<CBoolStatStruct> m_BoolData;
	atArray<CStringStatStruct> m_StringData;
	atArray<CUInt8StatStruct> m_UInt8Data;
	atArray<CUInt16StatStruct> m_UInt16Data;
	atArray<CUInt32StatStruct> m_UInt32Data;
	atArray<CUInt64StatStruct> m_UInt64Data;
	atArray<CInt64StatStruct> m_Int64Data;

	// PURPOSE:	Constructor
	CBaseStatsSaveStructure();

	// PURPOSE:	Destructor
	virtual ~CBaseStatsSaveStructure();

	void PreSaveBaseStats(const bool bMultiplayerSave, const unsigned saveCategory);
	void PostLoadBaseStats(const bool bMultiplayerSave);

	PAR_PARSABLE;
};


class CStatsSaveStructure : public CBaseStatsSaveStructure
{
public:
	//Track Vehicle Models Driven in Single Player Game.
	atArray< u32 > m_SpVehiclesDriven;

	//How many Peds of each type did the player kill.
	atBinaryMap<int, atHashString> m_PedsKilledOfThisType;

	//Store the stations play time ( station are organized by the audio code index).
	atBinaryMap<float, atHashString> m_aStationPlayTime;

	//Current count of Facebook driven number.
	u32 m_currCountAsFacebookDriven;

	//Vehicle Record data.
	atFixedArray< CStatsVehicleUsage, CStatsMgr::NUM_VEHICLE_RECORDS >  m_vehicleRecords;

public:
	// PURPOSE:	Constructor
	CStatsSaveStructure();

	// PURPOSE:	Destructor
	virtual ~CStatsSaveStructure();

#if __ALLOW_IMPORT_OF_SP_SAVEGAMES
	void Initialize(CStatsSaveStructure_Migration &copyFrom);
#endif // __ALLOW_IMPORT_OF_SP_SAVEGAMES

	void PreSave();
	void PostLoad();

	PAR_PARSABLE;
};

class CMultiplayerStatsSaveStructure : public CBaseStatsSaveStructure
{
public :
	//Track Vehicle Models Driven in Multi Player Game
	atArray< u32 > m_MpVehiclesDriven;

	// PURPOSE:	Constructor
	CMultiplayerStatsSaveStructure();

	// PURPOSE:	Destructor
	virtual ~CMultiplayerStatsSaveStructure();

	void PreSave();
	void PostLoad();

	PAR_PARSABLE;
};


//	#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
class CStatsSaveStructure_Migration	: public CBaseStatsSaveStructure
{
public:
	//Track Vehicle Models Driven in Single Player Game.
	atArray< u32 > m_SpVehiclesDriven;

	//How many Peds of each type did the player kill.
//	atBinaryMap<int, atHashString> m_PedsKilledOfThisType;
	atBinaryMap<int, u32> m_PedsKilledOfThisType;	//	Use u32 instead of atHashString as the key

	//Store the stations play time ( station are organized by the audio code index).
//	atBinaryMap<float, atHashString> m_aStationPlayTime;
	atBinaryMap<float, u32> m_aStationPlayTime;	//	Use u32 instead of atHashString as the key

	//Current count of Facebook driven number.
	u32 m_currCountAsFacebookDriven;

	//Vehicle Record data.
	atFixedArray< CStatsVehicleUsage, CStatsMgr::NUM_VEHICLE_RECORDS >  m_vehicleRecords;

public:
	// PURPOSE:	Constructor
	CStatsSaveStructure_Migration();

	// PURPOSE:	Destructor
	virtual ~CStatsSaveStructure_Migration();

	void Initialize(CStatsSaveStructure &copyFrom);

	void PreSave();
	void PostLoad();

#if  __ALLOW_EXPORT_OF_SP_SAVEGAMES || __ALLOW_IMPORT_OF_SP_SAVEGAMES
	s32 GetHashOfNameOfLastMissionPassed();
	float GetSinglePlayerCompletionPercentage();
#endif //  __ALLOW_EXPORT_OF_SP_SAVEGAMES || __ALLOW_IMPORT_OF_SP_SAVEGAMES

	PAR_PARSABLE;
};
//	#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES

#endif // SAVEGAME_DATA_STATS_H

