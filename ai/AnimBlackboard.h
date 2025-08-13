// FILE :    AnimBlackboard.h
// PURPOSE : Provides a place to track ambient animations as they play to prevent 
//						playing the same animation too often.
// AUTHOR :  Michael Dawe
// CREATED : 20 Dec 2012

#ifndef ANIM_BLACKBOARD_H
#define ANIM_BLACKBOARD_H

#define BLACKBOARD_SIZE 32

#include "atl/array.h"
#include "atl/singleton.h"
#include "vectormath/scalarv.h"

// System Includes
#include "fwsys/timer.h"

class CPedIntelligence; 

class CAnimBlackboard
{
public:
	// Different types of anims can define different time delays at which they can repeat. 
	//  Longer times for regular idles, shorter times for event reactions, etc.
	typedef enum {
		IDLE_ANIM_TIME_DELAY = 0,
		REACTION_ANIM_TIME_DELAY,
		MAX_BLACKBOARD_TIME_DELAY_TYPES
	} TimeDelayType;

	// Certain types of animations might be very similar even if they're not in the same clipset.
	// We'll use this enum to classify those types.
	typedef enum {
		NO_SPECIAL_ANIM,
		PANIC_EXIT,
		MAX_BLACKBOARD_SPECIAL_ANIM_TYPES
	} SpecialAnimType;

	struct AnimBlackboardItem 
	{
	public:
		AnimBlackboardItem() 
			: vLocation(V_ZERO)
			, uClipSetHash(0)
			, iClipIndex(0)
			, uTimePlayed(0) 
			, eTimeType(IDLE_ANIM_TIME_DELAY)
			, eSpecialType(NO_SPECIAL_ANIM) 
		{

		}
		explicit AnimBlackboardItem(u32 clipSetHash, u32 clipIndex, CAnimBlackboard::TimeDelayType timeType) 
			: vLocation(V_ZERO)
			, uClipSetHash(clipSetHash)
			, iClipIndex(clipIndex)
			, eTimeType(timeType)
			, eSpecialType(NO_SPECIAL_ANIM)
		{
			uTimePlayed = fwTimer::GetTimeInMilliseconds();
		}
		explicit AnimBlackboardItem(Vec3V_In location, CAnimBlackboard::SpecialAnimType specialType, CAnimBlackboard::TimeDelayType timeType)
			: vLocation(location)
			, uClipSetHash(0)
			, iClipIndex(0)
			, eTimeType(timeType)
			, eSpecialType(specialType)
		{
			uTimePlayed = fwTimer::GetTimeInMilliseconds();
		}


		bool operator==(const AnimBlackboardItem& rhs) const { return uClipSetHash == rhs.uClipSetHash && iClipIndex == rhs.iClipIndex; }
		
		Vec3V_ConstRef GetLocation() const { return vLocation; }
		u32 GetTimePlayed()  const { return uTimePlayed; }
		u32 GetClipSetHash() const { return uClipSetHash; }
		u32 GetClipIndex()   const { return iClipIndex; }
		CAnimBlackboard::TimeDelayType GetTimeType() const { return eTimeType; }
		CAnimBlackboard::SpecialAnimType GetSpecialType() const { return eSpecialType; }
	private:
		Vec3V vLocation;					// Location where this anim was played
		u32 uTimePlayed;					//Time this item was added
		u32 uClipSetHash;
		s32 iClipIndex;
		CAnimBlackboard::TimeDelayType eTimeType; 
		CAnimBlackboard::SpecialAnimType eSpecialType;
	
	};

	static void Init(unsigned initMode);
	static void Shutdown(unsigned shutdownMode);

	bool AddItemToBlackboardAndPedIntelligenceIfAllowedToPlay(const AnimBlackboardItem& item, CPedIntelligence* pPedIntelligence);

	static const ScalarV vMinSpecialTypeDistanceSq;
protected:
	CAnimBlackboard();
	CAnimBlackboard(const CAnimBlackboard&); //Purposely not implemented
	~CAnimBlackboard();

private:
	static CAnimBlackboard m_instance;

	int		FindAnimItemIndex(const AnimBlackboardItem& item) const;
	u32		TimeSinceLastPlayedAnim(const AnimBlackboardItem& item) const;
	bool  IsPlayingSpecialTypeWithinTimeAndDistance(const AnimBlackboardItem& item) const;
	bool	CanPlayAnim(const AnimBlackboardItem& item) const;
	void	AddItemToBlackboardAndPedIntelligence(const AnimBlackboardItem& item, CPedIntelligence* pPedIntelligence);


	atFixedArray<AnimBlackboardItem, BLACKBOARD_SIZE> m_blackboard;

	CompileTimeAssert(BLACKBOARD_SIZE < 256); // If this failed, m_uNewestIndex need to be larger than u8
	u8 m_uNewestIndex;

	static const u32 sm_MinTimeBetweenAnims[MAX_BLACKBOARD_TIME_DELAY_TYPES];
};

typedef atSingleton<CAnimBlackboard> CAnimBlackboardSingleton;
#define ANIMBLACKBOARD CAnimBlackboardSingleton::InstanceRef()

#endif
