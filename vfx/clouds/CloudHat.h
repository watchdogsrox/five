// 
// shadercontrollers/CloudHat.h 
// 
// Copyright (C) 1999-2008 Rockstar Games.  All Rights Reserved. 
// 

#ifndef RAGE_CLOUDHAT_H
#define RAGE_CLOUDHAT_H

#include "data/base.h"
#include "parser/manager.h"
#include "atl/functor.h"
#include "atl/atfunctor.h"
#include "atl/hashstring.h"
#include "vector/vector3.h"
#include "vector/vector2.h"
#include "vectormath/vec3v.h"

#if __BANK
#include "grcore/effect.h"  // for grcTexChangeData
#endif

#include "renderer/Lights/LightSource.h"
#include "vfx/clouds/clouds.h"

#define MAX_NUM_CLOUD_LIGHTS 4

//Forward Declare
namespace rage
{
	struct CloudFrame;
	class CloudHatManager;
	class CloudListController;
	class strStreamingModule;

#	if __BANK
		class bkBank;
		class bkGroup;
		class bkWidget;
#	endif //__BANK


	enum  CloudVars
	{
		SUN_DIRECTION,
		SUN_COLOR,
		CLOUD_COLOR,
		AMBIENT_COLOR,
		SKY_COLOR,
		BOUNCE_COLOR,
		EAST_COLOR,
		WEST_COLOR,
		DENSITY_SHIFT_SCALE,
		SCATTER_G_GSQUARED_PHASEMULT_SCALE,
		PIERCING_LIGHT_POWER_STRENGTH_NORMAL_STRENGTH_THICKNESS,
		SCALE_DIFFUSE_FILL_AMBIENT,
		WRAP_LIGHTING_MSAARef,
		NEAR_FAR_Q_MULT,

		//Animation CloudVars
		ANIM_COMBINE,
		ANIM_SCULPT,
		ANIM_BLEND_WEIGHTS,
		UV_OFFSET,
		VIEW_PROJ,
		CAMERA_POS,
		XENON_ONLY(DITHER_TEX,)
		DEPTHBUFFER,
		NUM_CLOUD_VARS
	};


class CloudVarCache
{
public: 
	
	CloudVarCache() : mCachedDrawable(NULL),mCacheInvalid(true) {ResetShaderVariables();}
	void ResetShaderVariables();
	void InvalidateCachedShaderVariables();
	void CacheShaderVariables(const rmcDrawable *drawable);

	template<class T>
	void SetCloudVar(grmShaderGroup &group, enum CloudVars index, const T &val)
	{
		Assertf(index >= 0 && index < NUM_CLOUD_VARS, "Trying to set invalid shader var index %d! Valid range: [0 - %d]", index, NUM_CLOUD_VARS);

		if(mCloudVar[index] != grmsgvNONE)	//Only set if shader variable exists.
		{
			group.SetVar(mCloudVar[index], val);
		}
	}

	template<class T>
	void SetCloudVar(grmShaderGroup &group, enum CloudVars index, const T &val, int count)
	{
		Assertf(index >= 0 && index < NUM_CLOUD_VARS, "Trying to set invalid shader var index %d! Valid range: [0 - %d]", index, NUM_CLOUD_VARS);

		if(mCloudVar[index] != grmsgvNONE)	//Only set if shader variable exists.
		{
			group.SetVar(mCloudVar[index], val, count);
		}
	}

#if RSG_PC || RSG_DURANGO || RSG_ORBIS
	// clear out W channel to prevent NaNs in shader vars
	template<class Vec3V>
	inline void SetCloudVar(grmShaderGroup &group, enum CloudVars index, Vec3V_In val)
	{
		Vec4V valT(val);
		valT.SetWf(0.0f);
		SetCloudVar(group, index, valT);
	}
#endif

private:
	typedef atRangeArray<grmShaderGroupVar, NUM_CLOUD_VARS> CloudVarArray;
	CloudVarArray mCloudVar;
	const rmcDrawable *mCachedDrawable;
	volatile bool mCacheInvalid;

	static const char *sCloudVarName[NUM_CLOUD_VARS];

#if NUMBER_OF_RENDER_THREADS > 1
	static sysCriticalSectionToken sCritSection;
#endif // NUMBER_OF_RENDER_THREADS > 1
};


//
//PURPOSE:
//	CloudHatFragLayer is a single layer of a composite cloudhat.

class CloudHatFragLayer : public datBase
{
public:
	typedef datBase base_type;
	friend class CloudHatManager;
	
	CloudHatFragLayer();
	virtual ~CloudHatFragLayer();

	void Update();
	void UpdateBufferedStates(int buf, float transitionMidPoint);

	bool IsStreamedIn() const;
	bool IsActive() const						{return mActive;}
	strLocalIndex  GetDrawableSlot() const		{return mDrawableSlot;}
	
	void Activate(bool preActivate = false);
	void Deactivate(bool instant = false);
	void PagedIn();

	float TransitionInUpdate(float cloudHatTransitionTime, float availableCost);
	float TransitionOutUpdate(float cloudHatTransitionTime, bool instant = false);
	bool  AltitudeUpdate(float height);

	void  StartTransitionIn(float transitionDuration, float alreadyElapsedTime, bool assumedMaxLength);
	void  StartTransitionOut(float transitionDuration, float alreadyElapsedTime, bool assumedMaxLength);

	float GetTransitionDuration() const			{return mTransitionDuration;}
	float GetTransitionStartTime() const		{return mTransitionStartTime;}
	float GetTransitionTimeElapsed() const;
	float GetTransitionPercent() const			{ return Clamp(GetTransitionTimeElapsed() / Max(GetTransitionDuration(), FLT_MIN), 0.0f, 1.0f); }
	bool  IsTransitioning() const				{ return mTransitioning; }
	bool  IsTransitioningIn() const				{ return IsTransitioning() && mTransitioningIn; }
	bool  IsTransitioningOut() const			{ return IsTransitioning() && !IsTransitioningIn(); }
	bool  IsStreamedInProcessed() const			{ return mHasStreamingRequestedPagedIn;}

	// render thread buffered functions
	bool IsActiveRender() const					{ return mRenderActive[1-CClouds::GetUpdateBuffer()]; }
	float GetRenderFadeAlpha() const			{ return mRenderTransitionAlpha[1-CClouds::GetUpdateBuffer()]; }
	rmcDrawable * GetRenderDrawable() const;
	CloudVarCache & GetShaderVarCache()			{ return mVariableCache; }
	bool IsAboveStreamInTrigger(float height) const;
	bool IsBelowStreamOutTrigger(float height) const;

public:
	atHashString mFilename;
	float mCostFactor;
	float mRotationScale;
	float mCamPositionScalerAdjust;
	float mTransitionInTimePercent;	// this in not a real time, but a percentage of the transition duration specified when the hat was requested
	float mTransitionOutTimePercent;
	float mTransitionInDelayPercent;	// this in not a real time, but a percentage of the transition duration specified when the hat was requested
	float mTransitionOutDelayPercent;
	float mHeightTigger;
	float mHeightFadeRange;
	float mHeightTrigger2;
	float mHeightFadeRange2;

	// not in the parser xml file:
	bool  mActive;
	bool  mPreloaded;
	bool  mHasStreamingRequestedPagedIn;
	strLocalIndex   mDrawableSlot;

	float mTransitionStartTime;
	float mTransitionDuration;
	float mTransitionDelay;

	bool  mTransitioningIn;
	bool  mTransitioning;

	CloudVarCache mVariableCache;
	
	// these are buffered for the render thread
	bool  mRenderActive[2];
	strLocalIndex   mRenderDrawableSlot[2];
	float mRenderTransitionAlpha[2];

	PAR_PARSABLE;
};


//
//PURPOSE:
//	CloudHatFragContainer is a container object for a single CloudHat. It contains
//	all the objects necessary to perform the streaming of the assets, non-weather related
//	settings, and code for managing shader variables, updating world matrix and drawing.

class CloudHatFragContainer : public datBase
{
public:
	typedef datBase base_type;
	friend class CloudHatManager;
	static const int sNumAnimLayers = 3;

	CloudHatFragContainer();
	virtual ~CloudHatFragContainer();

	void  Update(bool streamingOnly=false);
	void  Draw(const CloudFrame &settings, bool noDepthAvailable, const  grcTexture* depthBuffer, float globalAlpha);
	void  TransitionInUpdate(float availableCost);
	float TransitionOutUpdate(bool instant = false);
	void  AltitudeUpdate(float height);
	void  UpdateBufferedStates();

	void  Deactivate();

	void  PreLoad();
	void  DeactivatePreLoad();
	
	bool  AreAllLayersStreamedIn() const;
	bool  IsAnyLayerStreamedIn() const;
	bool  IsActive() const {return mActive;}
	
	bool  IsActiveRender() const {return mRenderActive[1-CClouds::GetUpdateBuffer()];}

	//Getter/Setters
	const char *			GetName() const					{ return mName; }
	Vec3V_Out				GetPosition() const				{ return mPosition; }
	Vec3V_Out				GetRotation() const				{ return mRotation; }
	Vec3V_Out				GetScale() const				{ return mScale; }
	float					GetTransitionAlphaRange() const	{ return mTransitionAlphaRange; }
	float					GetTransitionMidPoint() const	{ return mTransitionMidPoint; }

	//Transition info
	bool IsTransitioning() const				{ return mTransitioning; }
	bool IsTransitioningIn() const				{ return IsTransitioning() && mTransitioningIn; }
	bool IsTransitioningOut() const				{ return IsTransitioning() && !IsTransitioningIn(); }

	bool CalculatePositionOnCloudHat(Vector3 &out_position, const Vector2 &horizDirection) const;
#if __BANK
	void AddWidgets(bkBank &bank);
	void LoadFromWidgets();
	void UnloadFromWidgets();
	void PreloadFromWidgets();
	void ReleasePreloadFromWidgets();
	void UpdateWidgets();
	bkWidget *RemoveWidgets(bkBank &bank); //Returns group's next widget
	void UpdateTextures();
#endif

	enum { kMaxNameLength = 64 };
private:
	void ResetAnimState();	//Resets accumulated animation state to initial state.

	void StartTransitionIn(float transitionDuration, float alreadyElapsedTime = 0.0f, bool assumedMaxLength = false);
	void StartTransitionOut(float transitionDuration, float alreadyElapsedTime = 0.0f, bool assumedMaxLength = false);

#if __BANK
	void AddCloudHatWidgets(bkBank &bank);
	void AddMaterialGroup(bkBank &bank);
	void StartTransitionTest(bool transitionIn);
	void StartTransitionTestIn()	{ StartTransitionTest(true); }
	void StartTransitionTestOut()	{ StartTransitionTest(false); }
#endif

	//Parsed vars
	Vec3V mPosition;
	Vec3V mRotation;
	Vec3V mScale;
	char mName[kMaxNameLength];
	atArray<CloudHatFragLayer> mLayers;

	float mTransitionAlphaRange;
	float mTransitionMidPoint;
	bool mEnabled;

	//Streamed state
	bool mActive;
	bool mRenderActive[2]; 

	//Transition vars
	float mTransitionStartTime;
	bool mTransitioning;
	bool mTransitioningIn;

	//Animation Variables
	Vec3V mAngularVelocity; //For geometry rotation.
	Vec3V mAngularAccumRot; //Not parsed.
	Vec3V mAnimBlendWeights;
	Vec2V mUVVelocity[sNumAnimLayers];
	Vec2V mAnimUVAccum[sNumAnimLayers]; //Not parsed.
	u8 mAnimMode[sNumAnimLayers];
	bool mShowLayer[sNumAnimLayers];
	bool mEnableAnimations;

	Vec3V mAngularAccumRotBuffered[2];  // Buffered version for the render thread
	Vec2V mAnimUVAccumBuffered[2][sNumAnimLayers];

#if __BANK
	//Stuff for RAG UI
	bkGroup *mGroup;
	bkGroup *mMtlGroup;

	//Transition Testing
	float mTransitionTestT;
	float mTransTestDuration;
	bool mTransitionOverride;
	bool mTransitionTestInProgress;
	atArray<grcTexChangeData> BANKONLY_CloudTexArray;
#endif

	PAR_PARSABLE;
};


//
//PURPOSE:
//	CloudHatManager is the central location for managing all the CloudHat frags
//	and anything related to clouds.

class CloudHatManager : public datBase
{
public:
	typedef CloudHatManager this_type;
	typedef datBase base_type;
	typedef atArray<CloudHatFragContainer> array_type;
	typedef array_type::size_type size_type;
	typedef array_type::value_type value_type;
	typedef array_type::reference reference;
	typedef array_type::const_reference const_reference;
	typedef atFunctor0<bool> NullaryPredicate_type;
	typedef atFunctor0<size_type> NullaryFunctor_type;
	typedef atFunctor1<s32, const atHashString & > StreamingNameToSlotFunctor_type;

	CloudHatManager();

	// Static methods to manage singleton instance
	static void Create();
	static void Destroy();
	static this_type *GetInstance() { return sm_instance; }

	void Draw(const CloudFrame &settings, bool reflectionPhaseMode, float globalAlpha);
	void Update(bool skipWeather = false);

	//Push per-frame settings to cloud sets and compute all necessary shader variables.
	void SetCameraPos(Vec3V_In camPos) {mRealCameraPostion[g_RenderThreadIndex] = camPos;}
	Vec3V_Out GetCameraPos() const {return mRealCameraPostion[g_RenderThreadIndex];}

	////////
	//Interface into CloudHat frags.
	size_type GetNumCloudHats() const						{ return mCloudHatFrags.size(); }
	const_reference GetCloudHat(size_type index) const		{ return mCloudHatFrags[index]; }
	reference GetCloudHat(size_type index)					{ return mCloudHatFrags[index]; }
	array_type &GetCloudArray()								{ return mCloudHatFrags; }
	const array_type &GetCloudArray() const					{ return mCloudHatFrags; }
	size_type GetCurrentCloudHatIndex() const				{ return mCurrentCloudHat; }
	size_type GetNextCloudHatIndex() const					{ return mNextCloudHat; }

	size_type GetActiveCloudHatIndex() const;

	//Accessors
	bool IsCloudHatIndexValid(size_type index) const {return (index >= 0 && index < GetNumCloudHats());}

	////////
	//Pausing Simulation functionality. Set predicate for active monitoring or push value if you prefer.
	this_type &ClearSimulationPausedPredicate()										{ mSimulationPausedPredicate.Clear();	return *this; }
	this_type &SetSimulationPausedPredicate(const NullaryPredicate_type &predicate)	{ mSimulationPausedPredicate = predicate; return *this; }
	NullaryPredicate_type &GetSimulationPausedPredicate()							{ return mSimulationPausedPredicate; }
	//Passive push-functionality
	this_type &SetIsSimulationPaused(bool isPaused)									{ mIsSimulationPaused = isPaused; return *this; }
	//Returns whether the simulation is paused or not.
	//Note: If predicate is set, it will update the passive bool value and the correct state will be returned.
	bool IsSimulationPaused();

	Vec3V_ConstRef GetCamPositionScaler() const				{ return mCamPositionScaler; }
	float     GetAltitudeScrollScaler()   const				{ return mAltitudeScrollScaler; }

	////////
	//Functions for Transition management
	//Active transitions via Functors. Just set these functors to call a function that defines when a transition has occurred and if the transition
	//should be instantaneous and the cloud system will automatically monitor these conditions and initiate a transition when appropriate.
//	this_type &ClearTransitionFunctors()	{ mNameToSlotFunctor.Clear(); mTransitionPredicate.Clear(); mInstantTransitionPredicate.Clear(); mNextCloudHatFunctor.Clear(); return *this; }
	this_type &SetTransitionPredicate(const NullaryPredicate_type &predicate)				{ mTransitionPredicate = predicate;				return *this; }
	this_type &SetInstantTransitionPredicate(const NullaryPredicate_type &predicate)		{ mInstantTransitionPredicate = predicate;		return *this; }
	this_type &SetAssumedMaxLengthTransitionPredicate(const NullaryPredicate_type &predicate){ mAssumedMaxLengthTransitionPredicate = predicate; return *this; }
	this_type &SetNextCloudHatFunctor(const NullaryFunctor_type &func)						{ mNextCloudHatFunctor = func;					return *this; }
	this_type &SetStreamingNameToSlotFunctor(const StreamingNameToSlotFunctor_type &func)	{ mNameToSlotFunctor = func;					return *this; }
	//this_type &SetStreamingModule(strStreamingModule *streamingModule)						{ mStreamingModule = streamingModule;			return *this; }
	NullaryPredicate_type &GetTransitionPredicate()											{ return mTransitionPredicate; }
	NullaryPredicate_type &GetInstantTransitionPredicate()									{ return mInstantTransitionPredicate; }
	NullaryPredicate_type &GetAssumedMaxLengthTransitionPredicate()								{ return mAssumedMaxLengthTransitionPredicate; }
	NullaryFunctor_type &GetNextCloudHatFunctor()											{ return mNextCloudHatFunctor; }
	StreamingNameToSlotFunctor_type &GetStreamingNameToSlotFunctor()						{ return mNameToSlotFunctor; }

	//Passive transitions. If you would rather push transition update notifications to the cloud system, leave or reset the functors to the default
	//NullPredicate use this function to trigger transitions.
	void TriggerCloudTransition(size_type newCloud, bool isInstantanious = false);
	
    // activate/deactivate clouds hat by name. 
	// this is used by the script for trailers. 
	// It is not meant for real game play, please use cloud settings for that...
	void LoadCloudHatByName(const char *name, float transitionTime=0.0f);
	void UnloadCloudHatByName(const char * name, float transitionTime=0.0f);
	void UnloadAllCloudHats();

	void SetCloudLights(const CLightSource *lightSources);
	void ResetCloudLights()																	{ mUseCloudLighting = false; }

	//	new transition code
	void UpdateTransitions();
	void RequestWeatherCloudHat(int index, float transitionTime, bool assumedMaxLength);
	void RequestScriptCloudHat(int index,  float transitionTime );
	void ReleaseScriptCloudHat(int index,  float transitionTime );
	
	// the following lock functions 2 function are for script for special cutscene or trailers, they should be used with care!
	void PreloadCloudHatByName(const char *name);			// request a cloudhat be streaming streamed into memory. NOTE: it will be released when the CloudHat transitions out naturally or ReleasePreloadCloudHat() is called 
	void ReleasePreloadCloudHatByName(const char *name);	// release preloaded cloud hat 

	void Flush(bool out);

	float GetDesiredTransitionTimeSec() const { return mDesiredTransitionTimeSec; }

	int GetRequestedWeatherCloudHatIndex() const { return mRequestedWeatherCloudhat; }
	int GetRequestedScriptedCloudHatIndex() const { return mRequestedScriptCloudhat; }


#if __BANK
	typedef atFunctor1<void, bkBank & > BankFunctor_type;
	this_type &ClearBankFunctors()										{ mWidgetsChangedFunc.Clear(); return *this; }
	this_type &SetWidgetsChangedCbFunctor(const BankFunctor_type &func)	{ mWidgetsChangedFunc = func;	return *this; }
	BankFunctor_type &GetWidgetsChangedCbFunctor()						{ return mWidgetsChangedFunc; }

	static CloudFrame &GetCloudOverrideSettings();						//Debug functionality only!
	static void SetCloudOverrideSettings(const CloudFrame &settings);	//Debug functionality only!

	const char *GetCloudHatName(size_type index) const;
	bkBank &GetBank()			{ return *mBank; };
	bkGroup *GetMainGroup()		{ return mGroup; }
	bkGroup *GetDebugGroup()	{ return mDbgGroup; }
	float GetBankTransistionTime() {return mBankDesiredTransitionTimeSec;}

	void AddWidgets(bkBank &bank);
	void AddDebugWidgets(bkBank &bank);
	void ReloadWidgets();
	void UpdateWidgets();
	bool Save(const char *filename);
	void UpdateTextures();
	static const CloudFrame &OverrideFrameSettings(const CloudFrame &currFrame);
#endif

	bool Load(const char *filename, const char *ext = NULL);	//If ext is NULL, uses default XML extension
	//void UpdateRTM();

private:
	//Class Instance
	static CloudHatManager *sm_instance;

	//Fixed position modifier
	Vec3V mCamPositionScaler;

	float mAltitudeScrollScaler;
	
	array_type mCloudHatFrags;
	float mDesiredTransitionTimeSec;

	CLightSource m_cloudLightSources[MAX_NUM_CLOUD_LIGHTS];

	// New State Management. Moved up from the fragments
	int mRequestedWeatherCloudhat; // not the currently loaded but the currently requested by weather
	int mRequestedScriptCloudhat;  // if script has requested a cloud hat, we need to know (so we do not override it with weather)

#if GTA_REPLAY
	float m_PreviousTransitionTime;
	bool m_PreviousAssumedMaxLength;
#endif //GTA_REPLAY
		
	//Streaming Support
	//strStreamingModule *mStreamingModule;
	StreamingNameToSlotFunctor_type mNameToSlotFunctor;

	//For transitions
	NullaryPredicate_type mTransitionPredicate;
	NullaryPredicate_type mInstantTransitionPredicate;
	NullaryPredicate_type mAssumedMaxLengthTransitionPredicate;
	NullaryFunctor_type mNextCloudHatFunctor;
	size_type mCurrentCloudHat;
	size_type mNextCloudHat;

	//Simulation Paused Predicate
	NullaryPredicate_type mSimulationPausedPredicate;
	bool mIsSimulationPaused;
	bool mUseCloudLighting;
	Vec3V mRealCameraPostion[NUMBER_OF_RENDER_THREADS];

	//Transition functions
//	void TransitionToCloudHat(const size_type &nextSkyat);
//	void UpdateTransition();

#if __BANK
	//Stuff for RAG UI
	bkBank *mBank;
	bkGroup *mGroup;
	bkGroup *mDbgGroup;
	char mCurrCloudHatText[CloudHatFragContainer::kMaxNameLength];
	bool mBlockAutoTransitions; //useful for when you want to work on a specific CloudHat through Rag and don't want automatic transitions
	float mBankDesiredTransitionTimeSec;
	 
	//Functor to update game-level code and allow it to modify the widgets if necessary.
	BankFunctor_type mWidgetsChangedFunc;

	void AddCloudHatWidgetsAndIO(bkBank &bank);
	void UpdateCurrCloudHatText();

	static bool ms_bForceWireframe;
#endif

	PAR_PARSABLE;
};

} //namespace rage

#define CLOUDHATMGR (rage::CloudHatManager::GetInstance())

#endif // RAGE_CLOUDHAT_H 
