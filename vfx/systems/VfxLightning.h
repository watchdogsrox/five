///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	VfxLightning.h
//	BY	: 	Anoop Ravi Thomas
//	FOR	:	Rockstar Games
//	ON	:	29 November 2012
//	WHAT:	Lightning Effects
//
///////////////////////////////////////////////////////////////////////////////

#ifndef VFXLIGHTNING_H
#define VFXLIGHTNING_H

///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////													

// Rage headers


// Framework headers
#include "vfx/channel.h"

// Game headers
#include "control/replay/ReplaySettings.h"
#include "VfxLightningSettings.h"
#include "Vfx\clouds\CloudHat.h"
#include "timecycle\TimeCycle.h"

///////////////////////////////////////////////////////////////////////////////
//  FORWARD DECLARATIONS
///////////////////////////////////////////////////////////////////////////////	
class CVfxLightningStrike;

#if GTA_REPLAY
struct PacketLightningBlast;
struct PacketCloudBurst;
#endif // GTA_REPLAY

///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////		

//To be changed based on final settings chosen
#define		NUM_LIGHTNING_SEGMENTS						(243) //= (int)pow(3.0f, LIGHTNING_NUM_LEVEL_SUBDIVISIONS);
#define		NUM_LIGHTNING_SEGMENTS_SINGLE_SEGMENT_SPLIT	(3)

#define VFXLIGHTNING_UPDATE_BUFFER_ID (0)
#define VFXLIGHTNING_RENDER_BUFFER_ID (1)

///////////////////////////////////////////////////////////////////////////////
//  ENUMS
///////////////////////////////////////////////////////////////////////////////	

enum VfxLightningStrikeShaderVar_e
{
	VFXLIGHTNINGSTRIKE_SHADERVAR_PARAMS = 0,
	VFXLIGHTNINGSTRIKE_SHADERVAR_NOISE_PARAMS,
	VFXLIGHTNINGSTRIKE_SHADERVAR_NOISE_TEX,
	VFXLIGHTNINGSTIRKE_SHADERVAR_COUNT
};

///////////////////////////////////////////////////////////////////////////////
//  STRUCTURES
///////////////////////////////////////////////////////////////////////////////	

///////////////////////////////////////////////////////////////////////////////
//  SLightningSegment - Used for holding information about each 
//						lightning segment	
///////////////////////////////////////////////////////////////////////////////	
struct SLightningSegment
{
	Vector2 startPos;									// Start Position for the lightning segment
	Vector2 endPos;										// End Position for the lightning segment
	float startNoiseOffset;								// Start and End offsets for noise
	float endNoiseOffset;								// Start and End offsets for noise
	float width;										// Width of the lightning segment
	float Intensity;									// Intensity for the lightning segment (reducees with each branch)
	bool IsMainBoltSegment;								// Is Segment part of main bolt or a branch ?
	u8	distanceFromSource;								// Distance from the source of lightning (used for animation). Counts number of segments
};

typedef atFixedArray<SLightningSegment, NUM_LIGHTNING_SEGMENTS> SLightningSegments;
typedef atFixedArray<SLightningSegment, NUM_LIGHTNING_SEGMENTS_SINGLE_SEGMENT_SPLIT> SLightningSingleSegmentSplit;
///////////////////////////////////////////////////////////////////////////////
//  CLASSES
///////////////////////////////////////////////////////////////////////////////

// Lightning
struct LightningBlast
{
	Vector2 m_OrbitPos;
	float m_Intensity;
	float m_Duration;
	float m_Timer;
	float m_Size;
	float m_Distance;
};

struct LightingBurstSequence
{
	Vector2 m_OrbitRoot;
	float	m_Delay;
	int		m_BlastCount;
	LightningBlast m_LightningBlasts[MAX_LIGHTNING_BLAST];
};
///////////////////////////////////////////////////////////////////////////////
//  CVfxLighting - lightning strike
///////////////////////////////////////////////////////////////////////////////
class CVfxLightningStrike
{
public:
	struct SLightningSettings
	{
		SLightningSettings();
		Color32		Color;

	};

	CVfxLightningStrike();
	~CVfxLightningStrike();
	void Update(float deltaTime);
	void Render();
	void RenderCorona();
	bool IsDead();
	float GetBurstLightningIntensity();
	float GetMainLightningIntensity();

	static void RandomSeed(int value);
	static void InitShader();
	static void ShutdownShader();

	void Init();
	void Init(SLightningSettings &settings);
	void Shutdown();

	Vec3V_Out GetLightSourcePositionForClouds() const;
	const VfxLightningFrame &GetCurrentFrame() const { return m_currentFrame; }
	float GetMainIntensityKeyFrameLerp() const { return m_keyframeLerpMainIntensity; }
	float GetBranchIntensityKeyFrameLerp() const { return m_keyframeLerpBranchIntensity; }
	float GetBurstIntensityKeyFrameLerp() const { return m_keyframeLerpBurst; }
	Color32 GetColor() const	{return m_color;}
	float GetHeight() const { return m_Height;}
	float GetDistance() const { return m_distance; }
	float GetMainIntensity() const { return m_segments[0].Intensity; }
	float GetMainFlickerIntensity() const { return m_intensityFlickerMult; }
	bool IsBurstLightning() const { return m_isBurstLightning; }

#ifdef GTA_REPLAY
	const SLightningSegments& GetSegments() const { return m_segments; }
	SLightningSegments& GetSegments()  { return m_segments; }
	float GetDuration() const { return m_duration; }
	const Vector2& GetHorizDir() const { return m_horizDir; }
	float GetYaw() const { return m_rootYaw; } 
	float GetPitch() const { return m_rootPitch; }
	float GetAnimationTime() const { return m_AnimationTime; }

	void SetSegments(const SLightningSegments& segments) { m_segments = segments; }
	void SetDuration(const float duration) { m_duration = duration; }
	void SetMainIntensityKeyFrameLerp(float i) { m_keyframeLerpMainIntensity = i; } 
	void SetBranchIntensityKeyFrameLerp(float i) { m_keyframeLerpBranchIntensity = i; }
	void SetBurstIntensityKeyFrameLerp(float i) { m_keyframeLerpBurst = i; }
	void SetMainFlickerIntensity(float i) { m_intensityFlickerMult = i; }
	void SetColor(const Color32& color) { m_color = color; }
	void SetHeight(float height) { m_Height = height; }
	void SetDistance(float dist) { m_distance = dist; }
	void SetAnimationTime(float time) { m_AnimationTime = time; }
	void SetIsBurstLightning(bool b) { m_isBurstLightning = b; }
	void SetHorizDir(const Vector2& dir) { m_horizDir = dir; }
#endif // GTA_REPLAY

private:
	SLightningSegments			m_segments;							// Segments for lightning strike

	float						m_age;								// Current Age for the lightning
	float						m_duration;							// Duration for the lightning
	
	float						m_keyframeLerpMainIntensity;		// Used for choosing a value between min and max Main Intensity
	float						m_keyframeLerpBranchIntensity;		// Used for choosing a value between min and max Branch Intensity
	float						m_keyframeLerpBurst;				// Used for choosing a value between min and max Burst Intensity (directional light)
	float						m_intensityFlickerMultCurrent;		// Adding a flicker to the lightning intensity
	float						m_intensityFlickerMultNext;			// Adding a flicker to the lightning intensity
	float						m_intensityFlickerMult;				// Adding a flicker to the lightning intensity
	float						m_intensityFlickerCurrentTime;		
	Color32						m_color;							// Color for the lightning
	Vector2						m_horizDir;							// 2D Direction on XY Axis
	float						m_rootYaw;							// Yaw
	float						m_rootPitch;						//pitch
	float						m_Height;							// Height of the lightning
	float						m_distance;
	float						m_AnimationTime;					// Time for animating the striking hitting the ground
	int							m_variationIndex;					// Used for accessing the keyframe data
	bool						m_isBurstLightning;					// Is lightning Burst occuring with the strike? (directional Light)

	VfxLightningFrame			m_currentFrame;
	
	float Decay();
	float Decay(float amount);
	float Decay(float amount, u32 level);
	void GenerateLightning();
	void CreateSegment(SLightningSegment& out_segment, const Vector2 &startPos, const Vector2 &endPos, const u8 distanceFromSource, bool IsMainBoltSegment, bool isBranch, float currentWidth, float currentIntensity, float startNoiseoffset, float endNoiseOffset);
	void PatternFork(const SLightningSegment &segment, SLightningSingleSegmentSplit &output);
	void PatternZigZag(const SLightningSegment &segment, SLightningSingleSegmentSplit &output);
	
	void SplitSegments(SLightningSegments &segments, int fork);
	void SplitSegment(const SLightningSegment &inSegment, SLightningSingleSegmentSplit &outSegments, int fork);


	static grmShader*			m_vfxLightningStrikeShader;
	static grcEffectTechnique	m_vfxLightningStrikeShaderTechnique;
	static grcEffectVar			m_vfxLightningStrikeShaderVars[VFXLIGHTNINGSTIRKE_SHADERVAR_COUNT];
	static grcTexture*			m_vfxLightningStrikeNoiseTex;
	
};

///////////////////////////////////////////////////////////////////////////////
//  CVfxLightningManager - handles all lightning cases
///////////////////////////////////////////////////////////////////////////////
class CVfxLightningManager
{
public:

	CVfxLightningManager();
	~CVfxLightningManager();

	void Init(unsigned initMode);
	void InitShaders();
	static void InitDLCCommands();
	void ShutdownShaders();
	void Shutdown(unsigned shutdownMode);

	void SetupRenderthreadFrameInfo();
	void ClearRenderthreadFrameInfo();

	void Render();
	void Update(float deltaTime);

	void SetLightningChance(float lightningChance)		{ m_lightningChance = lightningChance; }
	float GetLightningChance()		{ return m_lightningChance; }

	void AddLightningStrike();
	void AddLightningStrike(CVfxLightningStrike::SLightningSettings &settings);

	void ForceLightningFlash				()	{ m_forceLightningFlash = true; }
	float GetLightningFlashAmount()			{ return m_lightningFlashAmount; }
	Vec3V_Out GetLightningFlashDirection()	{ return m_lightningFlashDirection; }
	
	__forceinline eLightningType GetLightningTypeUpdate()
	{
		vfxAssertf(CSystem::IsThisThreadId(SYS_THREAD_UPDATE), "CVfxLightningManager::GetLightningTypeUpdate - not called from the update thread");
		return m_currentLightningType[VFXLIGHTNING_UPDATE_BUFFER_ID];
	}
	__forceinline void SetLightningTypeUpdate(eLightningType lightningType)
	{
		vfxAssertf(CSystem::IsThisThreadId(SYS_THREAD_UPDATE), "CVfxLightningManager::SetLightningTypeUpdate - not called from the update thread");
		m_currentLightningType[VFXLIGHTNING_UPDATE_BUFFER_ID] = lightningType;
	}
	__forceinline eLightningType GetLightningTypeRender()
	{
		vfxAssertf(CSystem::IsThisThreadId(SYS_THREAD_RENDER), "CVfxLightningManager::GetLightningTypeRender - not called from the render thread");
		return m_currentLightningType[VFXLIGHTNING_RENDER_BUFFER_ID];
	}
	__forceinline void SetLightningTypeRender(eLightningType lightningType)
	{
		vfxAssertf(CSystem::IsThisThreadId(SYS_THREAD_RENDER), "CVfxLightningManager::SetLightningTypeRender - not called from the render thread");
		m_currentLightningType[VFXLIGHTNING_RENDER_BUFFER_ID] = lightningType;
	}

	void PerformTimeCycleOverride(CTimeCycle::frameInfo& currentFrameInfo, float alpha);

	const CVfxLightningStrike& GetCurrentLightningStrike() { return m_lightningStrike; }
	CVfxLightningStrike* GetCurrentLightningStrikePtr() { return &m_lightningStrike; }

	void			SetLightSourcesForClouds();
	void			CreateLightningSequence();
	void			CreateCloudLightningSequence();
	void			CreateCloudLightningSequence(float yaw, float pitch, bool bUseStrikeSettings);
	void			CreateDirectionalBurstSequence();
	void			UpdateCloudLightningSequence(float deltaTime, int index);
	void			UpdateDirectionalBurstSequence(float deltaTime);
	bool			IsCloudLightningActive()						{return m_IsCloudLightningActive;}
	void			UpdateAfterRender();
	
#if __BANK
	void AddLightningBurstInfoWidgets(bkBank* pBank);
	void InitWidgets();
	static void CreateVfxLightningWidgets();
	void AddLightningBurstDebugWidgets(bkBank* pBank);
#endif

#if GTA_REPLAY
	void			CreateDirectionalBurstSequence(const Vector2& rootOrbitPos, u8 blastCount, const PacketLightningBlast* pBlasts, const Vector3& direction, float flashAmount);
	void			CreateCloudLightningSequence(const Vector2& rootOrbitPos, u8 dominantIndex, u8 burstCount, const PacketCloudBurst* pBursts);

	CVfxLightningStrike& GetLightningStrike() { return m_lightningStrike; }
	void			SetLightningFlashDirection(Vec3V_In dir) { m_lightningFlashDirection = dir; }
	void			SetLightningType(u8 type) { SetLightningTypeUpdate((eLightningType)type); }
	void			SetLightningFlashAmount(float amount) { m_lightningFlashAmount = amount; }
#endif // GTA_REPLAY

	//ID3D11Buffer* GetLightningConstantBuffer() { return m_pcbLightning;}

	static CVfxLightningStrike*				m_lightningStrikeRender;

private:

#if GTA_REPLAY
	void			RecordReplayLightning();
#endif // GTA_REPLAY

	CVfxLightningStrike		m_lightningStrike;										// Single Lightning Strike
	LightingBurstSequence m_DirectionalBurstSequence;
	LightingBurstSequence m_CloudBurstSequence[MAX_NUM_CLOUD_LIGHTS];

	CLightSource m_lightSources[MAX_NUM_CLOUD_LIGHTS];
	bool				m_forceLightningFlash;									// force a single immediate lightning strike
	Vec3V				m_lightningFlashDirection;								// Indicates the direction of the bright flash
	eLightningType		m_currentLightningType[2];									// keep track of current lightning type. Double buffered to avoid threading issues
	float				m_lightningChance;										// Probability of lightning to occur (Set by weather.cpp)
	float				m_lightningFlashAmount;									// Flash Intensity amount
	u32					m_lightningStartTime;								// time that the lightning burst started
	Vector2				m_CloudBurstRootOrbitPos;
	Vector2				m_DirectionalBurstRootOrbitPos;
	bool				m_IsCloudLightningActive;
	
#if __BANK
	float				m_overrideLightning;									// override for the lightning value (<0.0 is off)
#endif

	int					m_numCloudLights;										//cache this value so replay can use it for recording.
};


extern CVfxLightningManager g_vfxLightningManager;

#endif
