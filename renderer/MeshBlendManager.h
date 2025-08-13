//
// renderer/MeshBlendManager.h
//
// Copyright (C) 2011-2013 Rockstar Games.  All Rights Reserved.
//
#ifndef _MESH_BLEND_MANAGER_H_
#define _MESH_BLEND_MANAGER_H_

#include "atl/vector.h"
#include "paging/ref.h"
#include "rmcore/drawable.h"
#include "squish/squishspu.h"

#include "stb/stb_dxt.h"
#include "streaming/streamingrequest.h"
#include "Scene/RegdRefTypes.h"

#if __BANK
namespace rage 
{
	class bkSlider;
}
#endif

typedef u32 MeshBlendHandle;
#define MESHBLEND_HANDLE_INVALID ((u32)-1)

#define MAX_ACTIVE_REQUESTS (321) // 2 slots + 3 slots for skin for each blend, plus an additional slot for the temp geom
#define DELAYED_DELETE_FRAMES (4)
#define DELAYED_FINALIZE_FRAMES (2)
#define IS_FINALIZED (-2)
#define MAX_OVERLAYS (13)
#define NUM_BODY_OVERLAYS (3)
CompileTimeAssert(MAX_OVERLAYS < 16); // need up update sBlendSlot::addedOverlayRefs otherwise

#define MAX_NUM_BLENDS (64) // number of players * 2 (clones in a corona)

enum eRampType
{
	RT_NONE = 0,
	RT_HAIR,
	RT_MAKEUP,
	RT_MAX
};

enum eMicroMorphType
{
	MMT_NOSE_WIDTH = 0,
	MMT_NOSE_HEIGHT,
	MMT_NOSE_LENGTH,
	MMT_NOSE_PROFILE,
	MMT_NOSE_TIP,
	MMT_NOSE_BROKE,
	MMT_BROW_HEIGHT,
	MMT_BROW_DEPTH,
	MMT_CHEEK_HEIGHT,
	MMT_CHEEK_DEPTH,
	MMT_CHEEK_PUFFED,
	MMT_EYES_SIZE,
	MMT_LIPS_SIZE,
	MMT_JAW_WIDTH,
	MMT_JAW_ROUND,
	MMT_CHIN_HEIGHT,
	MMT_CHIN_DEPTH,
	MMT_CHIN_POINTED,
	MMT_NUM_PAIRS,

	MMT_CHIN_BUM = MMT_NUM_PAIRS,
	MMT_NECK_MALE,
	MMT_SINGLES_END,

	MMT_MAX = MMT_SINGLES_END
};
#define MAX_MICRO_MORPHS (MMT_MAX)

#define NUM_MICRO_MORPH_ASSETS (38)
extern const char* s_microMorphAssets[NUM_MICRO_MORPH_ASSETS];
CompileTimeAssert(NUM_MICRO_MORPH_ASSETS == (MMT_NUM_PAIRS * 2 + (MMT_SINGLES_END - MMT_NUM_PAIRS)));

enum eOverlayBlendType
{
	OBT_ALPHA_BLEND = 0,
	OBT_OVERLAY,
	OBT_MAX
};

enum eMeshBlendTypes
{
	MBT_PED_HEADS,
	MBT_MAX
};

enum eMeshBlendState
{
	STATE_IDLE = 0,
	STATE_REQUEST_SLOT,
	STATE_INIT_RT,
	STATE_RENDERING,
	STATE_RENDER_BODY_SKIN,
	STATE_RENDER_OVERLAY,
	STATE_NOT_USED,
};

enum eSkinBlendComp
{
	SBC_FEET = 0,
	SBC_UPPR,
	SBC_LOWR,
	SBC_MAX
};
CompileTimeAssert(SBC_MAX < 8); // need up update sBlendSlot::addedSkinCompRefs otherwise
CompileTimeAssert(MAX_ACTIVE_REQUESTS == (SBC_MAX + 2) * MAX_NUM_BLENDS + 1);

enum eOverlayTextureType
{
	OTT_DIFFUSE,
	OTT_NORMAL,
	OTT_SPECULAR,
	OTT_MAX
};

#define MAX_RAMP_COLORS (64)

#define MAX_PALETTE_ENTRIES (64) // make sure these match the palette texture and the defines in skinblend.fx
#define MAX_PALETTE_COLORS (4)
class BlendPalette
{
public:
    BlendPalette();

    Color32 GetColor(s8 slot, u32 colIndex);
    void SetColor(s8 slot, u32 colIndex, Color32 color);

    s8 GetFreeSlot();
    void ReleaseSlot(s8 slot);

	bool IsUsed(s8 slot) const { return used[slot]; }

	void SetRefresh() { refresh = true; }
	bool NeedsRefresh() const { return refresh; }
	void Refreshed() { refresh = false; }

private:
    Color32 colors[MAX_PALETTE_ENTRIES][MAX_PALETTE_COLORS]; 
    bool used[MAX_PALETTE_ENTRIES];
	bool refresh;
};

class MeshBlendManager
{
public:
	static MeshBlendManager& GetInstance() { return *ms_instance; }

	static void Init(unsigned initMode);
	static void Shutdown(unsigned shutdownMode);
	static void Update();
	static void UpdateAtEndOfFrame();
	static void RenderThreadUpdate();

    static void ExtraContentUpdated(bool enable);

	static u32 GetRampCount(eRampType type) { return ms_rampCount[type]; }
	static Color32 GetRampColor(eRampType type, u32 index);

#if RSG_PC
	static bool IsAnyBlendActive();
#endif
	void FlushDelete();

	bool IsHandleValid(MeshBlendHandle handle) const;

	grcTexture* GetCrewPaletteTexture(MeshBlendHandle handle, s32& palSelector);
	grcTexture* GetTintTexture(MeshBlendHandle handle, eRampType rampType, s32& palSelector, s32& palSelector2);

	void RegisterPedSlot(s32 dwdIndex, s32 txdIndex, s32 feetTxdIndex, s32 upprTxdIndex, s32 lowrTxdIndex) { RegisterSlot(MBT_PED_HEADS, dwdIndex, txdIndex, feetTxdIndex, upprTxdIndex, lowrTxdIndex); }
	u32 GetNumPedSlots() const { return GetNumSlots(MBT_PED_HEADS); }
	MeshBlendHandle RequestNewPedHeadBlend(CPed* targetPed, s32 srcGeom1, s32 srcTex1, s32 srcGeom2, s32 srcTex2, s32 srcGeom3, s32 srcTex3, float geomBlend, float texBlend, float varBlend, bool async, bool male, bool parent) { return RequestNewBlend(MBT_PED_HEADS, targetPed, srcGeom1, srcTex1, srcGeom2, srcTex2, srcGeom3, srcTex3, geomBlend, texBlend, varBlend, async, male, parent); }
	bool UpdatePedHeadBlend(MeshBlendHandle handle, s32 srcGeom1, s32 srcTex1, s32 srcGeom2, s32 srcTex2, s32 srcGeom3, s32 srcTex3, float geomBlend, float texBlend, float varBlend) { return UpdateBlend(handle, srcGeom1, srcTex1, srcGeom2, srcTex2, srcGeom3, srcTex3, geomBlend, texBlend, varBlend); }
	void ReleaseBlend(MeshBlendHandle handle) { ReleaseBlend(handle, true); }
	MeshBlendHandle CloneBlend(MeshBlendHandle handle, CPed* targetPed);

	bool SetParentBlendData(MeshBlendHandle handle, s32 srcParentGeom1, s32 srcParentGeom2, float parentBlend1, float parentBlend2);

	void SetNewBlendValues(MeshBlendHandle handle, float geomBlend, float texBlend, float varBlend);
	void DoGeometryBlend(MeshBlendHandle handle);
	void ExtractGeometryTextures(MeshBlendHandle handle);

	void SetEyeColor(MeshBlendHandle handle, s32 colorIndex);
	u32 GetNumEyeColors() const { return m_eyeColorCols * m_eyeColorRows; }

	bool RequestNewOverlay(MeshBlendHandle handle, u32 slot, strLocalIndex txdIndex, eOverlayBlendType blendType, float blend, float normBlend);
	bool UpdateOverlayBlend(MeshBlendHandle handle, u32 slot, float blend, float normBlend);
	void ResetOverlay(MeshBlendHandle handle, u32 slot);
	void SetOverlayTintIndex(MeshBlendHandle handle, u32 slot, eRampType rampType, u8 tint, u8 tint2);

	void SetSkinBlendComponentData(MeshBlendHandle handle, eSkinBlendComp slot, s32 cloth, s32 skin0, s32 skin1, s32 skin2, s32 maskParentDwd, u8 palId);
	void RemoveSkinBlendComponentData(MeshBlendHandle handle, eSkinBlendComp slot);
	grcTexture* GetSpecTextureFromDrawable(rmcDrawable* drawable, const char* dbgName);
	grcTexture* GetPalTextureFromDrawable(rmcDrawable* drawable);

	void SetSpecAndNormalTextures(MeshBlendHandle handle, strLocalIndex drawable1, strLocalIndex drawable2);

	void SetHairBlendRampIndex(MeshBlendHandle handle, u8 rampIndex, u8 rampIndex2);

	bool SetRefreshPaletteColor(MeshBlendHandle handle);

	bool SetPaletteColor(MeshBlendHandle handle, bool enabled, u8 r = 0, u8 g = 0, u8 b = 0, u8 colorIndex = 0, bool bforce = false);
	void GetPaletteColor(MeshBlendHandle handle, u8& r, u8& g, u8& b, u8 colorIndex);

	s32 GetBlendTexture(MeshBlendHandle handle, bool mustBeLoaded, bool mustBeBlended);
	s32 GetBlendDrawable(MeshBlendHandle handle, bool mustBeLoaded, bool mustBeBlended);
	s32 GetBlendSkinTexture(MeshBlendHandle handle, eSkinBlendComp slot, bool mustBeLoaded, bool mustBeBlended, s32& dwdIndex);

	bool HasBlendFinished(MeshBlendHandle handle);
	void FinalizeHeadBlend(MeshBlendHandle handle) { FinalizeHeadBlend(handle, true); }
    bool IsBlendFinalized(MeshBlendHandle handle);
    void SetRequestsInProgress(MeshBlendHandle handle, bool val);

	u16 GetTypeFromHandle(MeshBlendHandle handle) const { return (u16)((handle >> 16) & 0xffff); }
	u16 GetIndexFromHandle(MeshBlendHandle handle) const { return (u16)(handle & 0xffff); }

	bool CompressDxt(grcRenderTarget* srcRt, grcTexture* dstTex, bool doMips);
	bool CompressDxtSTB(grcTexture* dst, grcRenderTarget* src, s32 numMips);

    bool IsWaitingForAssets(MeshBlendHandle handle) const;

    u32 GetPedCloneCount(MeshBlendHandle handle);
    CPed* GetPedClone(MeshBlendHandle handle, u32 cloneIndex);

	void MicroMorph(MeshBlendHandle handle, eMicroMorphType morphType, s32 storeIndex, float blend);

    void LockSlotRegistering() { m_bCanRegisterSlots = false; }

#if __BANK
	void AddWidgets(bkBank& bank);
	void SetActiveDebugEntry(MeshBlendHandle handle);
	void CalculateMemoryCost(MeshBlendHandle hnd, u32& ram, u32& vram, atArray<s32>& dwdAssets, atArray<s32>& txdAssets, u32& totalRam, u32& totalVram);

    void ForceTextureBlend(MeshBlendHandle handle);
	void ForceMeshBlend(MeshBlendHandle handle);

	static bool ms_showBlendTextures;
	static bool ms_showPalette;
    static bool ms_showNonFinalizedPeds;
	static bool ms_bypassMicroMorphs;
	static bool ms_showDebugText;
    static float ms_unsharpPass;
	static float ms_paletteScale;

#endif

private:
	struct sBlendSlot
	{
		sBlendSlot() : dwdIndex(-1), dwdReq(-1), txdIndex(-1), txdReq(-1), inUse(false), addedOverlayRefs(0), addedSkinCompRefs(0)
		{
			for (s32 i = 0; i < MAX_OVERLAYS; ++i)
			{
				overlayIndex[i] = -1;
				overlayReq[i] = -1;
			}

			for (s32 i = 0; i < SBC_MAX; ++i)
			{
				skinCompIndex[i] = -1;
				skinCompReq[i] = -1;
			}
		}

		grcTextureHandle headNorm;			// these four come from within the slot and head dwd so it's safe to keep raw pointers
		grcTextureHandle headSpec;
		grcTextureHandle headNormSrc;
		grcTextureHandle headSpecSrc;
		strLocalIndex dwdIndex;
		s32 dwdReq;
		strLocalIndex txdIndex;
		s32 txdReq;
		s32 overlayIndex[MAX_OVERLAYS];
		s32 overlayReq[MAX_OVERLAYS];
		strLocalIndex skinCompIndex[SBC_MAX];
		s32 skinCompReq[SBC_MAX];
		u16 addedOverlayRefs;
		u8 addedSkinCompRefs;
		bool inUse;
	};

	struct sSkinBlendInfo
	{
		s32 clothTex;
		s32 srcSkin0;
		s32 srcSkin1;
		s32 srcSkin2;
		s32 skinMaskParent;
		u8 palId;
		bool used;
	};
	struct sBlendEntry
	{
		sBlendEntry();

		grcRenderTarget* rt;
#if __D3D11 || RSG_ORBIS
		grcRenderTarget* rtTemp;
#endif
        atVector<RegdPed> clones;
		sSkinBlendInfo skinBlend[SBC_MAX];
		sqCompressState compressState;
		eMeshBlendState state;
		eMeshBlendTypes type;
		eOverlayBlendType blendType[MAX_OVERLAYS];
		float overlayAlpha[MAX_OVERLAYS];
		float overlayNormAlpha[MAX_OVERLAYS];
		u8 overlayTintIndex[MAX_OVERLAYS];
		u8 overlayTintIndex2[MAX_OVERLAYS];
		u8 overlayRampType[MAX_OVERLAYS];
		s32 slotIndex;
		float geomBlend;			// blend between the two parent heads
		float texBlend;				// blend between the two parent head textures
		float varBlend;				// "genetic variation" blend of a third head, not used in parent mode
		float parentBlend1;			// blend of first grandparent pair, used only in parent mode
		float parentBlend2;			// blend of second grandparent pair, used only in parent mode
		strLocalIndex srcTexIdx1;
		strLocalIndex srcTexIdx2;
		strLocalIndex srcTexIdx3;
		strLocalIndex srcGeomIdx1;
		strLocalIndex srcGeomIdx2;
		strLocalIndex srcGeomIdx3;
		strLocalIndex srcDrawable1;			// these two drawables are used to extract spec and normal maps
		strLocalIndex srcDrawable2;
		strLocalIndex srcParentGeomIdx1;
		strLocalIndex srcParentGeomIdx2;
        strLocalIndex microMorphGeom[MAX_MICRO_MORPHS];
        float microMorphAlpha[MAX_MICRO_MORPHS];
#if __BANK
		u32 startTime;
		u32 totalTime;
#endif
		u32 lastRenderFrame;
		u8 skinCompState;
		s8 deleteFrame;
		s8 mipCount;
		s8 refCount;
		s8 finalizeFrame;
        s8 paletteIndex;
        s8 eyeColorIndex;
		u8 hairRampIndex;
		u8 hairRampIndex2;
		bool reblendSkin[SBC_MAX];
		bool reblend;
		bool reblendMesh;
		bool reblendOverlayMaps;
		bool overlayCompSpec;
		bool addedRefs;
		bool async;
		bool male;
        bool hasReqs;
        bool usePaletteRgb;
		bool hasParents;
		bool parentBlendsFrozen;
		bool isParent;				// if this is a parent blend, it will be blended betwen the beauty/ugly heads, so need to swap spec/norm maps when blend crosses 0.5f value
		bool copyNormAndSpecTex;	// flag to indicate source heads have changed and normal and spec maps need to be copied
#if __BANK
		bool outOfMem;
#endif
	};

	MeshBlendManager();

	static bool IsLastRenderDone(const sBlendEntry& blend);

	static void RefreshPalette();
	static grcTexture* GetMiscTxdTexture(u32 ixd);

	static void RenderOverlay(MeshBlendHandle hnd, const sBlendEntry& blend, const sBlendSlot& slot, u32 idx);

	MeshBlendHandle GetHandleFromIndex(eMeshBlendTypes type, u16 index) const;

	bool AllocateRenderTarget(grcTexture* dst, grcTexture* dst2 OUTPUT_ONLY(, int slotId));
	void ReleaseRenderTarget();

	void ReleaseBlend(MeshBlendHandle handle, bool delayed);
	void FinalizeHeadBlend(MeshBlendHandle handle, bool delayed);

	u32 GetNumSlots(eMeshBlendTypes type) const { return m_slots[type].GetCount(); }

	void RegisterSlot(eMeshBlendTypes type, s32 dwdIndex, s32 txdIndex, s32 feetTxdIndex, s32 upprTxdIndex, s32 lowrTxdIndex);
	MeshBlendHandle RequestNewBlend(eMeshBlendTypes type, CPed* targetPed, s32 srcGeom1, s32 srcTex1, s32 srcGeom2, s32 srcTex2, s32 srcGeom3, s32 srcTex3, float geomBlend, float texBlend, float varBlend, bool async, bool male, bool parent);
	bool UpdateBlend(MeshBlendHandle handle, s32 srcGeom1, s32 srcTex1, s32 srcGeom2, s32 srcTex2, s32 srcGeom3, s32 srcTex3, float geomBlend, float texBlend, float varBlend);

	void CloneTexture(const grcTexture* src, grcTexture* dst) const;
	grcTexture* GetOverlayTexture(strLocalIndex txdIndex, eOverlayTextureType type) const;
	grcTexture* GetDrawableTexture(rmcDrawable* drawable, bool specular) const;

	void ReplaceAssets(sBlendEntry& blend, bool releaseOverlays, s32 newSrcTex1 = -1, s32 newSrcTex2 = -1, s32 newSrcTex3 = -1, s32 newSrcGeom1 = -1, s32 newSrcGeom2 = -1, s32 newSrcGeom3 = -1, bool finalizing = true);

    void UpdateEyeColorData();


	atFixedArray<atFixedArray<sBlendEntry, MAX_NUM_BLENDS>, MBT_MAX> m_blends;
	atFixedArray<atFixedArray<sBlendSlot, MAX_NUM_BLENDS + 1>, MBT_MAX> m_slots; // +1 for the intermediate one used when blending geometry

	atFixedArray<strRequest, MAX_ACTIVE_REQUESTS> m_reqs;

    BlendPalette m_palette;

	// we have a few different render target sizes that alias the same memory to accommodate the different textures we need to blend
	// - m_renderTarget is the base one which expects 256x256 textures
	// - m_renderTargetSmall is same as m_renderTarget minus one mip and is used for overlay normal/spec maps
	// - m_renderTargetPalette is for the custom ped palette
	// note: these sizes aren't strictly required for the source textures but the relationship between them is!
	grcRenderTarget* m_renderTarget;
	grcRenderTarget* m_renderTargetSmall;

#if __D3D11 || RSG_ORBIS
	grcRenderTarget* m_renderTargetTemp;
	grcRenderTarget* m_renderTargetSmallTemp;
#endif

#if RSG_ORBIS || RSG_DURANGO
	grcRenderTarget* m_renderTargetAliased256;
	grcRenderTarget* m_renderTargetAliased128;
	grcRenderTarget* m_renderTargetAliased64;
#elif RSG_PC
	atArray<grcRenderTarget*> m_tempDxtTargets;
	atArray<grcTexture*> m_tempDxtTextures;
	grcRenderTarget* FindTempDxtRenderTarget( const int nWidth, const int nHeight );
	grcTexture* FindTempDxtTexture( const int nWidth, const int nHeight );
#endif

	grcRenderTarget* m_renderTargetPalette;

	void DumpSlotDebug(void);

	u32 m_eyeColorRows;
	u32 m_eyeColorCols;
	float m_uStepEyeTex;
	float m_vStepEyeTex;

    bool m_bCanRegisterSlots;

	static MeshBlendManager* ms_instance;
	static s32 ms_intermediateSlotRefCount;

	static u32 ms_rampCount[RT_MAX];
	static Color32 ms_rampColors[RT_MAX][MAX_RAMP_COLORS];

#if !__FINAL
	static u16 ms_blendSlotBacktraces[MBT_MAX][MAX_NUM_BLENDS];
#endif //!__FINAL

#if __BANK
	static void ActiveBlendTypeChangedCB();
	static void DebugRender(s32 type, s32 blend);
	static void DebugText(s32 type, s32 blend, u32& totalRam, u32& totalVram, atArray<s32>& dwdAssets, atArray<s32>& txdAssets);

	static bkSlider* ms_activeBlendEntrySlider;
	static s32 ms_activeBlendEntry;
	static s32 ms_activeBlendType;
#endif

#if RSG_PC
	typedef enum 
	{
		DXT_COMPRESSOR_AVAILABLE = 0,
		DXT_COMPRESSOR_RESERVED,
	} eCompressionStatus;

	eCompressionStatus m_compressionStatus;
	grcTexture* m_pCompressionDest;
	u32 m_nCompressionMipStatusBits;

	bool FinishCompressDxt(bool bStallUntilFinished=false);			   // Render thread only
	void ReserveDxtCompressor(grcTexture* pDest, u32 nMipStatusBits);  // Render thread only
	void ReleaseDxtCompressor();                                       // Render thread only
	bool IsDxtCompressorReserved() const;							   // Main thread or Render thread

#endif // RSG_PC
};

#define MESHBLENDMANAGER MeshBlendManager::GetInstance()

#endif //_MESH_BLEND_MANAGER_H_
