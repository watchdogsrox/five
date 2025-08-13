//
//
//    Filename: TimeModelInfo.h
//     Creator: Adam Fowler
//     $Author: $
//       $Date: $
//   $Revision: $
// Description: Class describing a Time models that are displayed only at certain times of the day.
//
//
#ifndef INC_TIME_MODELINFO_H_
#define INC_TIME_MODELINFO_H_

// Game headers
#include "modelinfo\BaseModelInfo.h"

//
// name:		CTimeInfo
// description:	Information about models that change over time
class CTimeInfo
{
	enum	TimeInfoFlags
	{
		HOUR_BITMASK=0xffffff,
		HOUR_FLIP_WHILE_VISIBLE=1<<24
	};
public:
	CTimeInfo() : m_otherModel(fwModelId::MI_INVALID), m_hoursOnOff(0) {}

	// access functions
	inline void SetTimes(u8 timeOn, u8 timeOff) {
		s32 hour = timeOn;
		while(hour != timeOff)
		{
			m_hoursOnOff |= (1<<hour);
			hour++;
			if(hour == 24)
				hour = 0;
		}
	}
	inline void SetTimes(u32 timeHourFlags) {m_hoursOnOff=timeHourFlags;}

	inline void SetHoursOnOffMask(s32 onOffFlags) {m_hoursOnOff = onOffFlags;}
	inline s32 GetHoursOnOffMask() {return m_hoursOnOff;}
	inline bool IsOn(s32 hour) {return (m_hoursOnOff & (1<<hour))!=0;}
	
	CTimeInfo* FindOtherTimeModel(const char* pName);
	inline void SetOtherTimeModel(s32 index) { m_otherModel = index;}
	inline s32 GetOtherTimeModel() { return m_otherModel;}
	bool	OnlySwapWhenOffScreen(){ return ((m_hoursOnOff&(1<<24))==0);}
protected:
	u32 m_hoursOnOff;
	u32 m_otherModel;
};

//
// name:		CTimeModelInfo
// description:	Atomic model that switches on and off
class CTimeModelInfo : public CBaseModelInfo
{
public:
	CTimeModelInfo() {m_type = MI_TYPE_TIME;}

	virtual void InitArchetypeFromDefinition(strLocalIndex mapTypeDefIndex, fwArchetypeDef* definition, bool bHasAssetReferences);	
	virtual CTimeInfo* GetTimeInfo() {return &m_time;}
protected:
	CTimeInfo m_time;
};


#endif // INC_MLO_MODELINFO_H_
