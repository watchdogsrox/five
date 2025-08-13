#ifndef PLAYER_HORSE_FOLLOW_TARGETS_H
#define PLAYER_HORSE_FOLLOW_TARGETS_H

#include "Peds/Horse/horseDefines.h"

#if ENABLE_HORSE

#include "atl/array.h"
#include "scene/RegdRefTypes.h"

class CPhysical;


// It basically is just a wrapper for the fixed array of follow targets.
// The target selection logic lives in the behavior,
// and the activation logic lives in sagPlayerInputHorse -JM

class CPlayerHorseFollowTargets ///: public datBase
{
public:
	CPlayerHorseFollowTargets()
	{

	}

	enum FollowMode
	{
		kModeAuto,
		kModeSideOnly,
		kModeBehindOnly,
		kModeBehindAndSide,
		kModeBehindClose,
		kNumFollowModes
	};

	enum FollowPriority
	{
		kPriorityAmbient,
		kPriorityNormal,
		kPriorityHigh,
		kNumPriorities
	};

	struct FollowTargetEntry
	{
		FollowTargetEntry()
		{
			Reset();
		}

		void Reset()
		{
			offsetX = -1.0f;
			offsetY = -1.0f;
			disableWhenNotMounted = false;
			mode = kModeAuto;
			priority = kPriorityNormal;
			target.Reset(NULL);
			isPosse = false;
		}

		RegdConstPhy target;

		//a negative value indicates to the behavior
		//that it should figure out the offset automatically
		float offsetX;
		float offsetY;
		
		FollowMode mode;
		FollowPriority priority;
		bool disableWhenNotMounted;
		bool isPosse;

		bool operator==(const FollowTargetEntry& b2) const
		{
			return target == b2.target;
		}
	};

	void Reset();

	FollowTargetEntry& GetTarget(int i);
	int GetNumTargets() const
	{	return m_FollowTargets.GetCount();	}

	void AddTarget(const CPhysical* followTarget);
	void AddTarget(const CPhysical* followTarget, float offsX, float offsY, int followMode, int followPriority, bool disableWhenNotMounted, bool isPosse = false);
	void RemoveTarget(int i);
	void RemoveTarget(const CPhysical* followTarget);
	bool ContainsTarget(const CPhysical* followTarget) const;
	bool TargetIsPosseLeader(const CPhysical* followTarget) const;
	void ClearAllTargets();
	bool IsEmpty()
	{
		return m_FollowTargets.IsEmpty();
	}
	bool IsFull()
	{
		return m_FollowTargets.IsFull();
	}

	static const int kMaxFollowTargets = 8;

protected:
	atFixedArray<FollowTargetEntry, kMaxFollowTargets>	m_FollowTargets;
};


bool HelperIsDismounted(const CPhysical* targetActor);

#endif // ENABLE_HORSE

#endif	// #ifndef PLAYER_HORSE_FOLLOW_TARGETS_H
