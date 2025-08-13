//
// name:		Dynamic"scene/Entity.h"
// description:	Class description of a entity that is not static
#ifndef INC_DYNAMIC_ENTITY_H_
#define INC_DYNAMIC_ENTITY_H_

// Rage headers
#include "entity/sceneupdate.h"
#include "fragment/instance.h"
// Game headers
#include "animation/Move_config.h"
#include "crskeleton/skeleton.h"
#include "crskeleton/skeletondata.h"
#include "grblendshapes/blendshapes_config.h"
#include "network/Objects/Entities/NetObjGame.h"
#include "network/NetworkInterface.h"
#include "scene/portals/portalTracker.h"
#include "Cloth/ClothMgr.h"
#include "control/replay/ReplayMgrProxy.h"

class CGenericClipHelper;
class CMovePed;
class CPortalTracker;
class CSimpleBlendHelper;
class CWorldRepSectorArray;	


namespace rage {
	class crSkeleton;
	class crSkeletonData;
	class crCreature;
	class fwAnimDirector;
	class spdAABB;
	class grbTargetManager;
};

enum
{
	STATUS_PLAYER,
	STATUS_PHYSICS,
	STATUS_ABANDONED,
	STATUS_WRECKED,
	STATUS_PLAYER_DISABLED,
	STATUS_OUT_OF_CONTROL
};

enum ePopType
{
	POPTYPE_UNKNOWN = 0,

	// World simulation related population types.
	POPTYPE_RANDOM_PERMANENT,	// Trains, singularly Placed Helis, etc.
	POPTYPE_RANDOM_PARKED,		// Parked Vehs and boats, etc. (generated semi- randomly on car-generators)
	POPTYPE_RANDOM_PATROL,		// Peds and vehs generated on patrol paths (around banks, castles, etc).
	POPTYPE_RANDOM_SCENARIO,	// Random pairs of Peds talking in the street, etc. placed at scenario points.
	POPTYPE_RANDOM_AMBIENT,		// Random wandering Peds and Vehs (generated on road paths or the navmesh)

	// Game-play and playback related population types.
	POPTYPE_PERMANENT,			// The player and player groups.
	POPTYPE_MISSION,			// Scripted chars, vehs, etc.
	POPTYPE_REPLAY,				// For game replays.

	POPTYPE_CACHE,				// Vehicle is a part of the vehicle cache

	// Debugging and tool only population types.
	POPTYPE_TOOL,				// For debug tools.

	NUM_POPTYPES				// used by network code to calculate the number of bits needed to write a poptype
};

inline bool IsPopTypeRandom(const ePopType popType){return ((popType >= POPTYPE_RANDOM_PERMANENT) && (popType <= POPTYPE_RANDOM_AMBIENT));}
inline bool IsPopTypeRandomNonPermanent(const ePopType popType){return ((popType > POPTYPE_RANDOM_PERMANENT) && (popType <= POPTYPE_RANDOM_AMBIENT));}
inline bool IsPopTypeMission(const ePopType popType){return ((popType == POPTYPE_MISSION) || (popType == POPTYPE_PERMANENT));}

struct CDynamicEntityFlags
{
	friend class CDynamicEntity;

	u16		nStatus : 3;							// control status
	
	u16		bFrozenByInterior : 1;					// flag to say if an entity has been frozen by an interior
	u16		bFrozen : 1;							// Flag to say if the entity update has been frozen this frame (could be due to an interior, or a physical fixed flag)
	u16		bCheckedForDead : 1;					// The dead/alive state of this entity has been checked in the script code this frame
	u16		bIsGolfBall	: 1;				// the object is marked as a golf ball and will run some alternate or extra physics to deal with the small scale (Particularly for rolling)
	u16		bForcePrePhysicsAnimUpdate : 1;			// Set to true if the entity should always update its animation pre physics regardless of if it's on screen
	u16		bIsBreakableGlass : 1;
	u16		bIsOutOfMap : 1;
	u16		bOverridePhysicsBounds : 1;
	u16		bHasMovedSinceLastPreRender : 1;
	u16		bUseExtendedBoundingBox : 1;
	u16		bIsStraddlingPortal : 1;

	u16		nPopType : 4;
	u16		nPopTypePrev : 4;
	REPLAY_ONLY(u16     bReplayWarpedThisFrame : 1;)
};

CompileTimeAssert(NUM_POPTYPES <= 16);

//
//
//
class CDynamicEntity : public CEntity
{
#if GTA_REPLAY
	friend class CReplayInterfaceObject;
#endif 
public:
	CDynamicEntity(const eEntityOwnedBy ownedBy);
	~CDynamicEntity();

	// Add and remove from world
	void			AddToSceneUpdate();
	void			RemoveFromSceneUpdate();
	bool			GetIsOnSceneUpdate() const { return fwSceneUpdate::IsInSceneUpdate(*this); }
	void			AddSceneUpdateFlags(u32 flags);
	void			RemoveSceneUpdateFlags(u32 flags);
    bool            GetIsMainSceneUpdateFlagSet() const;

	virtual CDynamicEntity*	GetProcessControlOrderParent() const { return NULL; }

	virtual ePrerenderStatus PreRender(const bool bIsVisibleInMainViewport = true);
	virtual void PreRender2(const bool bIsVisibleInMainViewport = true);

	CEntityDrawInfo* CacheDrawInfo(float dist);

	virtual fwDrawData* AllocateDrawHandler(rmcDrawable* pDrawable);

	virtual void	DeleteDrawable();

	virtual void	SetHeading(float new_heading);
	virtual void	SetPosition(const Vector3& vec, bool bUpdateGameWorld = true, bool bUpdatePhysics = true, bool bWarp = false);
	virtual void	SetMatrix(const Matrix34& mat, bool bUpdateGameWorld = true, bool bUpdatePhysics = true, bool bWarp = false);

	// overridden functions that check if object is a fragment
	virtual Vector3	GetBoundCentre() const;
	virtual void	GetBoundCentre(Vector3& centre) const;	// quicker version
	virtual float	GetBoundCentreAndRadius(Vector3& centre) const; // get both in only 1 virtual function and help to simplify SPU code
	virtual float	GetBoundRadius() const;
	virtual const Vector3&	GetBoundingBoxMin() const;
	virtual const Vector3&	GetBoundingBoxMax() const;
	virtual FASTRETURNCHECK(const spdAABB &) GetBoundBox(spdAABB& box) const;
	virtual FASTRETURNCHECK(const spdAABB &) GetLocalSpaceBoundBox(spdAABB& box) const;
	float GetExtendedBoundCentreAndRadius(Vector3& centre) const; // get both in only 1 virtual function and help to simplify SPU code
	void GetExtendedBoundBox(spdAABB& box) const;


	virtual void		PopTypeSet(ePopType newPopType) { m_nDEflags.nPopTypePrev = m_nDEflags.nPopType; m_nDEflags.nPopType = newPopType; }
	ePopType			PopTypeGetPrevious() const { return (ePopType)m_nDEflags.nPopTypePrev; }
	ePopType			PopTypeGet() const { return (ePopType)m_nDEflags.nPopType; }
	bool				PopTypeIsRandom() const { return IsPopTypeRandom(PopTypeGet()); }
	bool				PopTypeIsRandomNonPermanent() const { return IsPopTypeRandomNonPermanent(PopTypeGet()); }
	bool				PopTypeIsMission() const { return IsPopTypeMission(PopTypeGet()); }

	void CreateSkeleton();
	eSkelMatrixMode GetSkelMode();

	// Return the expression set for this entity (if one is specified)
	virtual fwExpressionSet* GetExpressionSet();

	// Get the bone index from the bone tag
	// Returns -1 if the bone tag (or skeletondata) does not exist
	s32 GetBoneIndexFromBoneTag(const eAnimBoneTag boneTag) const;

	// Get the bone tag from the bone index
	// Returns BONETAG_INVALID if the bone tag (or skeletondata) does not exist
	eAnimBoneTag GetBoneTagFromBoneIndex(const int boneIndex) const;

	void UpdateSkeleton();
	void InverseUpdateSkeleton();

	virtual void UpdatePaused();

	virtual void StartAnimUpdate(float fTimeStep);
	virtual void StartAnimUpdateAfterCamera(float fTimeStep);
	virtual void EndAnimUpdate(float fTimeStep);
	virtual void UpdateVelocityAndAngularVelocity(float fTimeStep);

	// PURPOSE:	If updating through the animation queue, stop that and
	//			re-enable the regular scene update flags.
	// NOTES:	This is not meant to prevent the entity from switching back to use the animation queue.
	inline void StopUpdatingThroughAnimQueue();

	void CreateAnimDirector(rmcDrawable& drawable, bool addExtraDofsComponent = true, bool withRagDollComponent = false, bool withFacialRigComponent = false, CCustomShaderEffectBase* pCustomShaderEffectBase = NULL,  grbTargetManager *pgrbTargetManager = NULL);
#if ENABLE_BLENDSHAPES
	void AddBlendshapesCreatureComponent(rmcDrawable& drawable);
	void RemoveBlendshapesCreatureComponent();
#endif // ENABLE_BLENDSHAPES
	void AddShaderVarCreatureComponent(CCustomShaderEffectBase* pShaderEffect);
	void RemoveShaderVarCreatureComponent();

	virtual void UpdatePortalTracker();
	CPortalTracker* GetPortalTracker(void) {return(&m_portalTracker);}
	const CPortalTracker* GetPortalTracker(void) const {return(&m_portalTracker);}

	static inline void NextFrameForVisibility();
	inline void SetIsVisibleInSomeViewportThisFrame(bool b);
	bool GetIsVisibleInSomeViewportThisFrame() const { return sm_FrameCountForVisibility == m_FrameCountLastVisible; }

	/////////////////////////////////////////////////////////////////////////
	// script 

	// returns the thread that created this entity
	const class GtaThread* GetThreadThatCreatedMe() const;

	// returns the script id of the script that created this entity
	const class CGameScriptId* GetScriptThatCreatedMe() const;

	// unfortunately this is needed because CObject uses a different method to determine whether it is a mission entity
	virtual bool IsAScriptEntity() const { return PopTypeIsMission(); }

	// Sets up necessary state for a mission entity 
	virtual void SetupMissionState();

	// Cleans up any state set when this entity was a mission entity, reverting it back to a random entity.
	virtual void CleanupMissionState();

	// destroys the script extension, linking the entity to a script handler
	void DestroyScriptExtension();

	//////////////////////////////////////////////////////////////////////////

	// Gets a ranged random number based on the entity's RandomSeed
	u16	GetRandomSeed(void) const { return(m_randomSeed); }
	void SetRandomSeed(u16 seed) { m_randomSeed = seed; }
	float GetRandomNumberInRangeFromSeed(float a, float b) const;

	NETWORK_OBJECT_PTR_DECL_EX(CNetObjGame);	// +68; declares four bytes
    NETWORK_OBJECT_BASECLASS_DECL();

	static void InitClass();
	static void ShutdownClass();

	static void SetCanUseCachedVisibilityThisFrame(bool b) { sm_CanUseCachedVisibilityThisFrame = b; }
	static bool GetCanUseCachedVisibilityThisFrame() { return sm_CanUseCachedVisibilityThisFrame; }

	// flag stuff (moved from CEntity)
	CDynamicEntityFlags		m_nDEflags;			// +72

	inline void SetStatus(s32 nStatus) {
		if (m_nDEflags.nStatus==STATUS_WRECKED && nStatus!=STATUS_WRECKED && !NetworkInterface::IsGameInProgress() REPLAY_ONLY(&& CReplayMgrProxy::IsEditModeActive() == false)) 	
		{Assertf(0, "Error - Trying to change the status of a vehicle from being wrecked");}
		else
		{m_nDEflags.nStatus = nStatus;}
	}
	inline s32 GetStatus() const { return m_nDEflags.nStatus; }
	inline bool TreatAsPlayerForCollisions() { if (m_nDEflags.nStatus == STATUS_PLAYER){return true;}else{return false;}}

	// Override this to send any additional needed move signals immediately before the post camera move state update occurs
	// This will only be called if the entity was not updated in the pre physics update
	virtual void OnEnterViewport() { return; }

	void AddToStraddlingPortalsContainer();
	void RemoveFromStraddlingPortalsContainer();
	void UpdateStraddledPortalEntityDesc();
	virtual bool CanStraddlePortals() const			{ return false; }
	bool IsStraddlingPortal() const					{ return m_nDEflags.bIsStraddlingPortal == 1;}
	u8 GetPortalStraddlingContainerIndex() const	{ return m_portalStraddlingContainerIndex; }
	virtual bool CanLeaveInteriorRetainList() const	{ return true; }

protected:
	virtual void	Add();
	virtual void	Remove();
	virtual void	AddToInterior(fwInteriorLocation interiorLocation);
	virtual void	RemoveFromInterior();

	// Apply bone overrides specified in the animation section of RAG
	DEV_ONLY(void ApplyBoneOverrides());

	void DeleteAnimDirector();

	u16	m_randomSeed;	// A random number set upon creation that is used for flickering lights and other random events//32

	// PURPOSE:	The value of sm_FrameCountForVisibility when this entity was last found
	//			to be visible. sm_FrameCountForVisibility will wrap around, m_FrameCountLastVisible is meant
	//			to get rejuvenated before then.
	u8 m_FrameCountLastVisible;
	
	u8 m_portalStraddlingContainerIndex;

	u8 m_pad[10];

	CPortalTracker m_portalTracker;		// necessary for tracking moving objects through interiors...

	// PURPOSE:	Static frame counter used with m_FrameCountLastVisible to keep track
	//			of whether this entity is currently visible or not without having to reset
	//			anything each frame.
	static u8 sm_FrameCountForVisibility;

	// PURPOSE:	This gets set to false during camera cuts, so we can check it before calling
	//			GetIsVisibleInSomeViewportThisFrame() between the camera update and the prerender pass,
	//			when it's lagging one frame behind.
	static bool sm_CanUseCachedVisibilityThisFrame;

private:
	__forceinline class rage::phInst* GetBoundBoxInline(spdAABB& box) const;

public:
#if __BANK
	virtual void LogBaseFlagChange(s32 nFlag, bool bProtected, bool bNewVal, bool bOldVal);	
#endif
};

inline s32 CDynamicEntity::GetBoneIndexFromBoneTag(const eAnimBoneTag boneTag) const
{
	int boneIndex = -1;
	if(GetSkeleton())
	{
		GetSkeletonData().ConvertBoneIdToIndex((u16)boneTag, boneIndex);
	}
	return boneIndex;
}

inline eAnimBoneTag CDynamicEntity::GetBoneTagFromBoneIndex(const int boneIndex) const
{
	eAnimBoneTag boneTag = BONETAG_INVALID;
	if(GetSkeleton())
	{
		boneTag = (eAnimBoneTag)GetSkeletonData().GetBoneData(boneIndex)->GetBoneId();
	}
	return boneTag;
}

inline void CDynamicEntity::StopUpdatingThroughAnimQueue()
{
	if(GetUpdatingThroughAnimQueue())
	{
		SetUpdatingThroughAnimQueue(false);
		AddSceneUpdateFlags(GetMainSceneUpdateFlag());
	}
}

inline void CDynamicEntity::NextFrameForVisibility()
{
	// Note: this is expected to wrap around after 255.
	sm_FrameCountForVisibility++;
}

inline void CDynamicEntity::SetIsVisibleInSomeViewportThisFrame(bool b)
{
	if(b)
	{
		m_FrameCountLastVisible = sm_FrameCountForVisibility;
	}
	else
	{
		m_FrameCountLastVisible = sm_FrameCountForVisibility - 1;
	}
}

inline float CDynamicEntity::GetRandomNumberInRangeFromSeed(float a, float b) const 
{ 
	return (((float)(m_randomSeed)/65535.0f) * (b-a)) + a; 
}

#endif

