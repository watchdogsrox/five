//
// renderer/PedHeadshotManager.h
//
// Copyright (C) 2012-2014 Rockstar Games.  All Rights Reserved.
//
// Handles headshot requests of player peds
//
#ifndef _PED_HEADSHOT_MANAGER_H_
#define _PED_HEADSHOT_MANAGER_H_

// rage includes
#include "vector/vector3.h"
#include "vector/vector4.h"
#include "vector/color32.h"
#include "math/float16.h"
#include "grcore/allocscope.h"
#include "grcore/effect.h"
#include "grcore/device.h"
#include "grmodel/shadergroupvar.h"
#include "grmodel/matrixset.h"
#include "paging/ref.h"
#include "atl/hashstring.h"
#include "atl/vector.h"
#include "rline/cloud/rlcloud.h"

#if !__WIN32PC
#include "squish/squishspu.h"
#endif

// framework includes
#include "entity/archetypemanager.h"
#include "fwscene/stores/txdstore.h"
#include "fwanimation/clipsets.h"

// game includes
#include "scene/RegdRefTypes.h"
#include "renderer/Lights/LightSource.h"
#include "scene/RegdRefTypes.h"
#include "task/System/TaskHelpers.h"
#include "Network/Cloud/CloudManager.h"

#define PHM_RT_RESIDENT (1 && (RSG_PC || RSG_DURANGO || RSG_ORBIS))

namespace rage
{
	class grcViewport;
	class rmcDrawable;
	class bkBank;
	class fragInst;
	class phIntersection;
	class crSkeleton;
	class crSkeletonData;
	class grcTexture;
	class grcRenderTarget;
	class Color32;
	class grmShader;
	class bkCombo;
};	

class CPed;

enum PedHeadshotState
{
	PHS_INVALID = 0,
	PHS_QUEUED,
	PHS_READY,
	PHS_WAITING_ON_TEXTURE,
	PHS_FAILED,
};

struct PedHeadshotInfo {
	PedHeadshotInfo()  { Reset(); };

	void				Reset();

	RegdPed				pPed;
	RegdPed				pSourcePed;

	grcTextureHandle	texture;
	strLocalIndex		txdId;
	u32					hash;
	int					refCount;
	bool				isRegistered;
	bool				isInProcess;
	bool				isActive;
	bool				hasTransparentBackground;
	bool				forceHiRes;
	bool				isWaitingOnRelease;
	bool				refAdded;
	bool				isPosed;
	bool				isCloned;
	PedHeadshotState	state;
};

typedef int PedHeadshotHandle;

class PedHeadshotTextureBuilder
{
public:
	enum eState
	{
		kNotReady = 0,
		kAcquired,
		kWaitingOnTexture,
		kWaitingOnRender,
		kWaitingForCompression,
		kWaitingForGpuCompression,
		kCompressing,
		kCompressionSubmitted,
		kCompressionWait,
		kCompressionWaitForFinish,
		kCompressionFinished,
		kOutputTextureReleased,
		kFailed,
	};


	PedHeadshotTextureBuilder() :	m_pRTCol0(NULL),m_pRTCol0Small(NULL), m_pRTCol1(NULL), m_pRTColCopy(NULL), m_pRTDepth(NULL), 
									m_RTPoolIdCol(0), m_RTPoolIdDepth(0), 
									m_state(kNotReady), m_dstTextureRefAdded(false),
									m_bRenderTargetAcquired(false), m_alphaThreshold(0), m_bUseGpuCompression(false), m_bUseSmallRT(false) {};
	
	bool Init();
	void Shutdown();

	bool ProcessHeadshot(atHashString dstTXD, atHashString dstTEX, bool bHasAlpha, bool bForceHiRes);
	bool IsReadyForRendering() const;
	bool IsCompressionFinished() const;
	bool IsUsingGpuCompression() const { return m_bUseGpuCompression; }
	void KickOffGpuCompression();

	bool IsUsingSTBCompressionRenderThread() { return m_bUseSTBCompressionRenderThread; }
	void KickOffSTBCompressionRenderThread();

	void Abort();
	void SetReadyForCompression();
	void SetReadyForGpuCompression();

	bool HasFailed() const		{ return m_state == kFailed; }
	void RecoverFromFailure();

	grcTexture*			GetTexture(bool& bUsedGpuCompression);
	grcRenderTarget*	GetColourRenderTarget0();
	grcRenderTarget*	GetColourRenderTarget1();
	grcRenderTarget*	GetColourRenderTargetCopy();
	grcRenderTarget*	GetDepthRenderTarget();
	
	void Update();
	void UpdateRender();

	enum
	{
		kRtAligment = 4096,
		kRtMips = 1,
		kRtWidth = 128, 
		kRtHeight = 128, 
		kRtColourBpp = 32, 
		kRtPitch = kRtWidth*kRtColourBpp/8,
	};

private:

#if __XENON
	void UntileTarget();
#endif

	void KickOffCompression();
	void UpdateCompressionStatus();
	void UpdateTextureStreamingStatus();
	void AllocateRenderTargetPool();
	void ReleaseRenderTargetPool();

	bool AllocateRenderTarget(bool bHasAlpha, bool bForceHiRes);
	void ReleaseRenderTarget();
#if PHM_RT_RESIDENT
	bool PreAllocateRenderTarget();
	void PostReleaseRenderTarget();
#endif

	void ReleaseRefs();



#if !__WIN32PC
	sqCompressState			m_textureCompressionState;
#endif


	atHashString			m_txdName;
	atHashString			m_texName;
	grcTextureHandle		m_dstTexture;
	eState				 	m_state;
	grcRenderTarget*	 	m_pRTCol0;
	grcRenderTarget*		m_pRTCol1;
	grcRenderTarget*	 	m_pRTColCopy;
#if PHM_RT_RESIDENT
	grcRenderTarget*	 	m_pRTCol1L;
	grcRenderTarget*	 	m_pRTCol1S;
#endif

#if __D3D11
	grcTexture*				m_pRTCol1Texture;
#endif
	grcRenderTarget*	 	m_pRTDepth;
	grcRenderTarget*	 	m_pRTCol0Small;
	size_t				 	m_RTPoolSizeCol;
	size_t				 	m_RTPoolSizeDepth;
	u16					 	m_RTPoolIdCol;
	u16					 	m_RTPoolIdDepth;
	bool					m_dstTextureRefAdded;
	bool					m_bRenderTargetAcquired;
	bool					m_bUseGpuCompression;
	bool					m_bUseSmallRT;
	u8						m_alphaThreshold;
	bool					m_bUseSTBCompressionRenderThread;
};

// Helper class to export headshot textures to the cloud.
// It can only deal with one request at a time and relies
// on the user to release a request (whatever its state is)
// so that it becomes available for further requests.
class PedHeadshotTextureExporter : public CloudListener
{
public:

	struct TextureData 
	{
		TextureData() { Reset(); }
		void Reset() { width = 0U; height = 0U; pitch = 0U; imgFormat = 0U; isValid = false; }

		u32					width;
		u32					height;
		u32					pitch;
		u32					imgFormat;
		bool				isValid;
	};

	enum State 
	{
		AVAILABLE = 0,
		REQUEST_IN_PROGRESS,
		REQUEST_UPLOADING,
		REQUEST_WAITING_ON_UPLOAD,
		REQUEST_FAILED,
		REQUEST_SUCCEEDED,
	};

	PedHeadshotTextureExporter() { Reset(); }

	void	Init();
	void	Shutdown();

	bool	RequestUpload(s32 txdId, const char* pCloudPath);
	void	ReleaseRequest(s32 txdId);
	void	Update();

	bool	HasRequestFailed() const { return m_state == REQUEST_FAILED; };
	bool	HasRequestSucceeded() const { return m_state == REQUEST_SUCCEEDED; };
	bool	IsAvailable() const { return m_state == AVAILABLE; };

	// CloudListener callback
	void	OnCloudEvent(const CloudEvent* pEvent);

#if __BANK
	void	UpdateDebug();
	static void	AddWidgets	(rage::bkBank& bank);
#endif

private:

	bool 	PrepareRequestData();
	void	ReleaseRequestData();

	State	ProcessDDSBuffer();
	State	ProcessUploadRequest();
	State	ProcessFailedRequest();

	u32		GetDDSHeaderSize() const;

	bool	CanProcessRequest() const;
	void	Reset();

#if __BANK
	void	SaveDDSToDisk();

	static bool ms_bSaveToDisk;
	static bool ms_bDontUpload;
	static bool ms_bReleaseRequest;
	static bool ms_bTriggerRequest;
	static bool ms_bUseGameCharacterSlot;
	static s32	ms_txdSlot;
#endif


	char				m_fileName[RAGE_MAX_PATH];
	State				m_state;
	CloudRequestID		m_requestId;
	strLocalIndex		m_txdSlot;
	s32					m_txdSlotReleaseCheck;

	bool				m_bReleaseRequested;

	grcTextureHandle	m_texture;
	TextureData			m_textureData;

	void*				m_pWorkBuffer0;
#if __XENON
	void*				m_pWorkBuffer1;
#endif

};


class DeferredRendererWrapper
{

public:

	DeferredRendererWrapper() {};

	void Init(grcRenderTarget* pGBufferRTs[4], grcRenderTarget* pDepthRT, grcRenderTarget* pDepthAliasRT, grcRenderTarget* pLightRT, grcRenderTarget* pLightCopyRT, grcRenderTarget* pCompositeRT, grcRenderTarget* pCompositeRTCopy, grcRenderTarget* pDepthCopyRT
#if __D3D11
		,grcRenderTarget* pStencilRT
#endif
		);
	void Shutdown();

	// General interface
	static grcRenderTarget* GetTarget(DeferredRendererWrapper* pRenderer, u32 index)															{ return pRenderer->_GetTarget(index); }
	static grcRenderTarget* GetDepth(DeferredRendererWrapper* pRenderer)																		{ return pRenderer->_GetDepth(); }
	static grcRenderTarget* GetLightingTarget(DeferredRendererWrapper* pRenderer)																{ return pRenderer->_GetLightingTarget(); }
	static grcRenderTarget* GetDepthAlias(DeferredRendererWrapper* pRenderer)																	{ return pRenderer->_GetDepthAlias(); }
	static grcRenderTarget* GetCompositeTarget(DeferredRendererWrapper* pRenderer)																{ return pRenderer->_GetCompositeTarget(); }

	static grcRenderTarget* GetLightingTargetCopy(DeferredRendererWrapper* pRenderer)															{ return pRenderer->_GetLightingTargetCopy(); }
	static grcRenderTarget* GetCompositeTargetCopy(DeferredRendererWrapper* pRenderer)															{ return pRenderer->_GetCompositeTargetCopy(); }

	static void SetUIProjectionParams(DeferredRendererWrapper* pRenderer, eDeferredShaders currentShader)										{ pRenderer->_SetUIProjectionParams(currentShader); }
	static void SetUILightingGlobals(DeferredRendererWrapper* pRenderer)																		{ pRenderer->_SetUILightingGlobals(); }
	static void RestoreLightingGlobals(DeferredRendererWrapper* pRenderer)																		{ pRenderer->_RestoreLightingGlobals(); };

	static void Initialize(DeferredRendererWrapper* pRenderer, const grcViewport* pVp, CLightSource* pLightSources, u32 numLights)				{ pRenderer->_Initialize(pVp, pLightSources, numLights); }
	static void Finalize(DeferredRendererWrapper* pRenderer)																					{ pRenderer->_Finalize(); }

	static void DeferredRenderStart(DeferredRendererWrapper* pRenderer)																			{ pRenderer->_DeferredRenderStart(); }
	static void DeferredRenderEnd(DeferredRendererWrapper* pRenderer)																			{ pRenderer->_DeferredRenderEnd(); }

	static void ForwardRenderStart(DeferredRendererWrapper* pRenderer)																			{ pRenderer->_ForwardRenderStart(); }
	static void ForwardRenderEnd(DeferredRendererWrapper* pRenderer)																			{ pRenderer->_ForwardRenderEnd(); }

	static void DeferredLight(DeferredRendererWrapper* pRenderer)																				{ pRenderer->_DeferredLight(); }

	static void SetAmbients(DeferredRendererWrapper* pRenderer)																					{ pRenderer->_SetAmbients(); } 

	static void RenderToneMapPass(DeferredRendererWrapper* pRenderer, grcRenderTarget* pDst, grcTexture* pSrc, Color32 col, bool bClearDst)		{ pRenderer->_RenderToneMapPass(pDst, pSrc, col, bClearDst); }
	static void SetToneMapScalars(DeferredRendererWrapper* pRenderer)																			{ pRenderer->_SetToneMapScalars(); }

	static void LockRenderTargets(DeferredRendererWrapper* pRenderer, bool bLockDepth)															{ pRenderer->_LockRenderTargets(bLockDepth); }
	static void UnlockRenderTargets(DeferredRendererWrapper* pRenderer, bool bResolveCol, bool bResolveDepth)									{ pRenderer->_UnlockRenderTargets(bResolveCol, bResolveDepth); }

	static void LockSingleTarget(DeferredRendererWrapper* pRenderer, u32 index, bool bLockDepth)												{ pRenderer->_LockSingleTarget(index, bLockDepth); }
	static void LockLightingTarget(DeferredRendererWrapper* pRenderer, bool bLockDepth)															{ pRenderer->_LockLightingTarget(bLockDepth); }
	static void UnlockSingleTarget(DeferredRendererWrapper* pRenderer, bool bResolveColor, bool bClearColor, bool bClearDepth)					{ pRenderer->_UnlockSingleTarget(bResolveColor, bClearColor, bClearDepth); }
	static void SetGbufferTargets(DeferredRendererWrapper* pRenderer)																			{ pRenderer->_SetGbufferTargets(); }

	// Helpers
	static void InitialisePedHeadshotRenderer(DeferredRendererWrapper* pRenderer);
#if RSG_PC
	static void CopyTarget(DeferredRendererWrapper* pRenderer, grcRenderTarget* pDst, grcRenderTarget* pSrc);
#endif // RSG_PC
private:

	grcRenderTarget* _GetTarget(u32 index)		{ return m_pGBufferRTs[index]; }
	grcRenderTarget* _GetDepth()				{ return m_pDepthRT; }
#if __D3D11
	grcRenderTarget* _GetStencil()				{ return m_pStencilRT; } 
#endif
	grcRenderTarget* _GetLightingTarget()		{ return m_pLightRT; }	
	grcRenderTarget* _GetDepthAlias()			{ return m_pDepthAliasRT; }
	grcRenderTarget* _GetCompositeTarget()		{ return m_pCompositeRT; }

	grcRenderTarget* _GetLightingTargetCopy()	{ return (m_pLightRTCopy != NULL) ? m_pLightRTCopy : m_pLightRT; }
	grcRenderTarget* _GetCompositeTargetCopy()		{ return (m_pCompositeRTCopy != NULL) ? m_pCompositeRTCopy : m_pCompositeRT; }
	grcRenderTarget* _GetDepthCopy();
	
	void _SetUIProjectionParams(eDeferredShaders currentShader);
	void _SetUILightingGlobals();
	void _RestoreLightingGlobals();

	void _Initialize(const grcViewport* pVp, CLightSource* pLightSources, u32 numLights);
	void _Finalize();

	void _DeferredRenderStart();
	void _DeferredRenderEnd();

	void _ForwardRenderStart();
	void _ForwardRenderEnd();

	void _DeferredLight();

	void _RenderToneMapPass(grcRenderTarget* pDstRT, grcTexture* pSrcRT, Color32 col, bool bClearDst);
	void _SetToneMapScalars();

	void _LockRenderTargets(bool bLockDepth);
	void _UnlockRenderTargets(bool bResolveCol, bool bResolveDepth);

	void _LockSingleTarget(u32 index, bool bLockDepth);
	void _LockLightingTarget(bool bLockDepth);
	void _UnlockSingleTarget(bool bResolveColor, bool bClearColor, bool bClearDepth);
	void _SetGbufferTargets();

	void _SetAmbients();
	
	GRC_ALLOC_SCOPE_SINGLE_THREADED_DECLARE(,m_AllocScope)

	grcRenderTarget*	m_pGBufferRTs[4];
	grcRenderTarget*	m_pDepthRT;
#if __D3D11
	grcRenderTarget*	m_pStencilRT;
#endif
	grcRenderTarget*	m_pDepthAliasRT;
	grcRenderTarget*	m_pLightRT;
	grcRenderTarget*	m_pCompositeRT;

	grcRenderTarget*	m_pLightRTCopy;
	grcRenderTarget*	m_pCompositeRTCopy;
#if RSG_PC
	grcRenderTarget*    m_pDepthCopy;
#endif // RSG_PC

	s32		m_prevForcedTechniqueGroup;
	bool	m_prevInInterior;

	grcDepthStencilStateHandle m_DSSWriteStencilHandle;
	grcDepthStencilStateHandle m_DSSTestStencilHandle;
	grcDepthStencilStateHandle m_insideLightVolumeDSS;
	grcDepthStencilStateHandle m_outsideLightVolumeDSS;

	CLightSource*	m_pLightSource;
	u32				m_numLights;

	grcViewport		m_viewport;
	grcViewport*	m_pLastViewport;




};

class PedHeadshotManager 
{

public:

	enum { kMaxPedHeashots = 34 };
	enum { kMaxHiResPedHeashots = 7 };						// 1st texture is hires alpha, then 7 hires textures, rest is lores
	enum { kMaxPedHeashotRenderTargets = kMaxPedHeashots };
	enum { kHeadshotWidth = 128, kHeadshotHeight = 128 };

	PedHeadshotHandle	RegisterPed(CPed* pPed, bool bNeedTransparentBackground = false, bool bForceHiRes = false);	
	PedHeadshotHandle	RegisterPedWithAnimPose(CPed* pPed, atHashString animClipDictionaryName, atHashString animClipName, Vector3& camPos, Vector3& camAngles, bool bNeedTransparentBackground = false);

#if __XENON
	grcRenderTarget*	GetParentTarget() { return DeferredRendererWrapper::GetLightingTarget(&m_renderer); } 
#endif

	void				UnregisterPed(PedHeadshotHandle handle);

	PedHeadshotHandle	GetRegisteredWithTransparentBackground() const;

	PedHeadshotState	GetPedHeashotState(PedHeadshotHandle handle) const;

	u32					GetRegisteredCount() const;
	int					GetNumRefsforEntry(PedHeadshotHandle handle) const;
	int					GetTextureNumRefsforEntry(PedHeadshotHandle handle) const;

	u32					GetTextureHash(PedHeadshotHandle handle) const;
	const char*			GetTextureName(PedHeadshotHandle handle) const;

	bool				IsRegistered(PedHeadshotHandle handle) const;
	bool				IsAnyRegistered() const;
	bool				IsActive(PedHeadshotHandle handle) const;
	bool				IsValid(PedHeadshotHandle handle) const { return (handle != INVALID_HANDLE) && IsRegistered(handle); };
	bool				HasTransparentBackground(PedHeadshotHandle handle) const;
	grcTexture*			GetTexture(PedHeadshotHandle handle);

	void				SetUseCustomLighting(bool bEnable);
	void				SetCustomLightParams(int index, const Vector3& pos, const Vector3& col, float intensity, float radius);

	bool				CanRequestPoseForPed() const { return (sm_bPosedPedRequestPending == false); };
	void				AddToDrawList();
	void				PreRender();
	bool				HasPedsToDraw() const;

	// Texture cloud upload
	bool				RequestTextureUpload(PedHeadshotHandle handle, int forcedCharacterId = -1);
	void				ReleaseTextureUploadRequest(PedHeadshotHandle handle);
	bool				IsTextureUploadAvailable() const { return m_textureExporter.IsAvailable(); }
	bool				HasTextureUploadFailed() const { return m_textureExporter.HasRequestFailed(); }
	bool				HasTextureUploadSucceeded() const { return m_textureExporter.HasRequestSucceeded(); }
	
	void				IncNumFramesWaitingForRender() { m_numFramesWaitingForRender++; }
	void				ResetNumFramesWaitingForRender() { m_numFramesWaitingForRender = 0; }

	static void	RenderUpdate();
	static void	UpdateSafe();
	static void Render();


	PedHeadshotManager() {};
	~PedHeadshotManager(){};

	static void Init();
	static void Shutdown();

	static PedHeadshotManager* smp_Instance;
	static PedHeadshotManager& GetInstance() { return *smp_Instance; }


	static void SetupViewport();
	static void SetupLights();
	static void DrawListPrologue();
	static void DrawListEpilogue();
	static void RunCompression();
	static void RunSTBCompressionRenderThread();
	static void DebugRender();
	static void SetRenderCompletionFence();
	static void SetReadyForCompression();
	static void SetReadyForGpuCompression();

#if __BANK
	PedHeadshotHandle	RegisterPedAuto(CPed* pPed);	
	void				UnregisterPedAuto(CPed* pPed);
	void				SetSourcePed(PedHeadshotHandle handle, CPed* pPed);

	static const char*	GetDebugInfoStr(PedHeadshotHandle handle);
	static void			AddWidgets	(rage::bkBank& bank);
	static void			UpdateDebug();
	static bool			IsAutoRegisterPedEnabled() { return sm_bAutoRegisterPed; }
#endif 

private:
	static void CompositePass(grcRenderTarget* pDst, grcRenderTarget* pSrc);
	static void AAPass(grcRenderTarget* pDst, grcRenderTarget* pSrc);

	void AnimationUpdate(CPed* pPed);

	PedHeadshotHandle	RegisterPedWithAnimPose(CPed* pPed, bool bNeedTransparentBackground = false);

	void MarkClonePedForDeletion(PedHeadshotInfo& entry);

	static void InitLightSources();

	static void BuildDummyMatrixSet();

	const atVector<PedHeadshotInfo, kMaxPedHeashots>&	GetEntriesPool() const	{ return m_entriesPool; };

	bool				IsTextureAvailableForSlot(int idx) const;

	int					HandleToSlotIndex(PedHeadshotHandle handle) const		{ return (int)handle-1; };
	PedHeadshotHandle	SlotIndexToHandle(int idx) const						{ return (PedHeadshotHandle)(idx+1); };

	void OnFailure(PedHeadshotInfo& entry);

	bool InitPosedPed(PedHeadshotInfo& entry);
	bool InitClonedPed(PedHeadshotInfo& entry);
	bool CleanUpPedData();


	bool IsRenderingFinished();
	void UpdateRenderCompletionFenceStatus();
	void Update();
	bool UpdateCurrentEntry();
	bool UpdateTextureBuilderStatus();
	bool ProcessReleasedEntries();
	bool ProcessPendingEntries();
	void ReleaseEntry(PedHeadshotInfo& entry);

	bool IsCurrentEntryReadyForRendering();

	bool AddHeadshotToDrawList(grcRenderTarget* pRtCol0, grcRenderTarget* pRtCol1, grcRenderTarget* pRtColCopy, grcRenderTarget* pRtDepth, const PedHeadshotInfo& headshotInfo);
	void AddPedToDrawList(CPed* pPed);

	bool CanProcessEntry(const PedHeadshotInfo& entry) const;
	void ResetCurrentEntry();


	void AddRefToCurrentEntry();

	void AddRefToEntry(PedHeadshotInfo& entry);
	void RemoveRefFromEntry(PedHeadshotInfo& entry);

	static void RenderToneMapPass(grcRenderTarget* pRT, grcRenderTarget* pDepthTarget, grcTexture* pSrcTexture);

	PedHeadshotTextureExporter										m_textureExporter;
	PedHeadshotTextureBuilder										m_textureBuilder;
	atVector<PedHeadshotInfo, kMaxPedHeashots>						m_entriesPool;
	PedHeadshotInfo*												m_pCurrentEntry;

	grcViewport*													m_pViewport;

	atFixedArray<RegdPed, kMaxPedHeashots>							m_delayedDeletionPed;

	enum AnimDataRequestState {
		kClipHelperReady = 0,
		kClipHelperWaitingOnData,
		kClipHelperDataLoaded,
		kClipHelperDataWaitingForRendering,
	};

	// Animation
	fwClipSetRequestHelper											m_headShotAnimRequestHelper;

	DeferredRendererWrapper											m_renderer;

	CClipRequestHelper												m_clipRequestHelper;
	AnimDataRequestState											m_animDataState;
	atHashString													m_clipDictionaryName;
	atHashString													m_clipName;

	grcFenceHandle													m_renderCompletionFence;

	enum RenderState {
		kRenderIdle = 0,
		kRenderBusy,
		kRenderDone,
	};
	volatile RenderState											m_renderState;

	int																m_numFramesWaitingForRender;
	static int														sm_maxNumFramesToWaitForRender;

	static s32														sm_prevForcedTechniqueGroup;
	static Vec3V													sm_posedPedCamPos;
	static Vec3V													sm_posedPedCamAngles;
	static bool														sm_bPosedPedRequestPending;

	static const int												INVALID_HANDLE;

	static grmMatrixSet*											sm_pDummyMtxSet;

	static CLightSource												sm_customLightSource[4];	// script can set these up
	static CLightSource												sm_defLightSource[4];		// default lights setting
	static Vector4													sm_ambientLight[2];
	static char														sm_textureNameCache[16];
	static bool														sm_bUseCustomLighting;

	// camera settings
	static float													sm_defaultVFOVDeg;
	static float													sm_defaultAspectRatio;
	static float													sm_defaultCamHeightOffset;
	static float													sm_defaultCamDepthOffset;

	static float													sm_defaultExposure;

#if __BANK

	const char* GetNameForEntry(PedHeadshotHandle handle) const;
	const char* GetNameForEntry(const PedHeadshotInfo& entry) const;
	int			GetSlotIndexFromName(atHashString inName) const;

	void		UpdateEntryNamesList(const atVector<PedHeadshotInfo, kMaxPedHeashots>& entries);
	void		OnEntryNameSelected();

	static void DebugLightsCallback();
	static void UpdateDebugLights();

	struct PedHeadshotDebugLight
	{
		PedHeadshotDebugLight() : intensity(0.0f), radius(0.0f), isDisabled(false) {}

		Vector3		pos;
		float		intensity;
		float		radius;
		Color32		color;
		bool		isDisabled;
	};

	static PedHeadshotDebugLight	sm_debugLights[4];
	static bool						sm_bEditLights;
	static bool						sm_bSyncLights;
	static bool						sm_dumpLightSettings;
	static bool						sm_bAddHeadshot;
	static bool						sm_bContinousHeadshot;
	static bool						sm_bDiplayActiveHeadshots;
	static bool						sm_bClearHeadshots;

	static bool						sm_bLockInHeadshotRendering;
	static bool						sm_bSkipRenderOpaque;
	static bool						sm_bSkipRenderAlpha;
	static bool						sm_bSkipRenderCutout;

	static bool						sm_bDoNextHeadshotWithTransparentBackground;
	static bool						sm_bReleaseHeadshot;

	static bkCombo*					sm_pEntryNamesCombo;
	static int						sm_entryNamesComboIdx;
	static int						sm_selectedEntryIdx;
	static bool						sm_bNextPedWithPose;

	static bool						sm_bForceNullPedPointerDuringRender;
	static int						sm_numFramesToDelayTextureLoading;
	static int						sm_numFramesToDelayTextureLoadingPrev;
	static int						sm_numFramesToSkipSafeWaitForCompression;

	static bool						sm_bUsePedsFromPicker;
	static atArray<const char*>		sm_entryNames;
	static char						sm_debugStr[64];
	static bool						sm_bAutoRegisterPed;
	static bool						sm_bSkipRendering;
	static bool						sm_bReleaseNextCurrentEntry;
	static float					sm_maxSqrdRadiusForAutoRegisterPed;
	static float					sm_debugDrawVerticalOffset;
	static bool						sm_bMultipleAddToDrawlist;
#endif

	friend class PedHeadshotTextureBuilder;
	friend class PedHeadshotTextureExporter;
};

//////////////////////////////////////////////////////////////////////////
// Temporary helper class for gpu compression
class GpuDXTCompressor
{

public:

	static void Init();
	static void Shutdown();

	static bool AllocateRenderTarget(grcTexture* pDestTexture);
	static void ReleaseRenderTarget();

	static bool CompressDXT1(grcRenderTarget* pSrcRt, grcTexture* pDstTex);
#if RSG_PC
	static grcRenderTarget* FindTempDxtRenderTarget( const int nWidth, const int nHeight );
	static grcTexture* FindTempDxtTexture( const int nWidth, const int nHeight );
	static bool FinishCompressDxt();
	static bool RequiresFinishing() {return m_pCompressionDest != NULL;}
#endif
private:

	static grcRenderTarget* ms_pRenderTargetAlias;

	static grmShader*			ms_pShader;
	static grcEffectTechnique	ms_compressTechniqueId;
	static grcEffectVar 		ms_texture3VarId;
	static grcEffectVar 		ms_texelSizeVarId;
	static grcEffectVar 		ms_compressLodVarId;
#if RSG_PC
	static grcRenderTarget*	 	ms_pRTDxtCompS;
	static grcRenderTarget*	 	ms_pRTDxtCompL;
	static grcTexture*		 	ms_pDxtCompS;
	static grcTexture*	 		ms_pDxtCompL;

	static grcTexture*			m_pCompressionDest;
	static u32					m_nCompressionMipStatusBits;
#endif
};






#define PEDHEADSHOTMANAGER PedHeadshotManager::GetInstance()

#endif //_PED_HEADSHOT_MANAGER_H_
