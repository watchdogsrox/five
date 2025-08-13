//
// filename:	vehicleDamage.h
// description:	
//

#ifndef INC_VEHICLEDAMAGE_H_
#define INC_VEHICLEDAMAGE_H_

// --- Include Files ------------------------------------------------------------

// C headers

// Rage headers
#include "grmodel/shaderGroupVar.h"
#include "phcore/materialMgr.h"
#include "phbound/surfacegrid.h"
#include "vector/vector3.h"
#include "vector/Vector4.h"
#include "vectormath/vectormath.h"
#include "math/simplemath.h"
#include "fwsys/glue.h"
#include "pedsafezone/pedsafezone.h"
#include "grcore/device.h"
#include "renderer/ApplyDamage.h"

// Game headers
#include "scene/RegdRefTypes.h"
#include "vehicles/door.h"
#include "control/replay/ReplaySettings.h"
#include "weapons/WeaponEnums.h"
#include "math/intrinsics.h"

class CEntity;
class CVehicle;

namespace rage
{
	class grcRenderTarget;
	class grcTexture;
}

// --- Defines ------------------------------------------------------------------
#include "shader_source/vehicles/vehicle_common_values.h"

#define VEHICLE_DEFORMATION_NOISE_SMOOTHING_OCTAVES (2)
#define VEHICLE_DEFORMATION_COLOR_VAR_MULT_MIN	(ScalarVConstant<FLOAT_TO_INT(0.70f)>())
#define VEHICLE_DEFORMATION_COLOR_VAR_MULT_MAX	(ScalarVConstant<FLOAT_TO_INT(1.15f)>())
#define VEHICLE_DEFORMATION_COLOR_VAR_ADD_MIN	(ScalarVConstant<FLOAT_TO_INT(-0.05f)>())
#define VEHICLE_DEFORMATION_COLOR_VAR_ADD_MAX	(ScalarVConstant<FLOAT_TO_INT(0.05f)>())

#if GPU_DAMAGE_WRITE_ENABLED

#define VEHICLE_DEFORMATION_TEXTURE_ARRAY_SIZE (2 * MAX_GPUS)

#define VEHICLE_DEFORMATION_TEXTURE_CACHE_SIZE_INIT	(MAX_DAMAGED_VEHICLES)	// first init value, initialized for real in VehicleDeformation::TexCacheInit()
#define VEHICLE_DEFORMATION_IMPACT_STORE_SIZE		(MAX_IMPACTS_PER_VEHICLE_PER_FRAME)

// In DX11, blending over SNORM target is prohibited for some reason. The debug runtime issues an ERROR but everything seems to work correctly.
// Enable this for running with debug runtime if you want to work around this error.
#define VEHICLE_DEFORMATION_USE_HALF_FLOATS		(0 && RSG_PC)

# if VEHICLE_DEFORMATION_USE_HALF_FLOATS
#include "half.h"
typedef uint16_t half;

struct TexelValue_A16B16G16R16F
{
	half red;
	half green;
	half blue;
	half alpha;
};
# else
struct TexelValue_R8G8B8A8_SNORM
{
	s8 red;
	s8 green;
	s8 blue;
	s8 alpha;	
};
#endif //VEHICLE_DEFORMATION_USE_HALF_FLOATS

#else
#define VEHICLE_DEFORMATION_TEXTURE_CACHE_SIZE_INIT	(16)	// first init value, initialized for real in VehicleDeformation::TexCacheInit()
#define VEHICLE_DEFORMATION_IMPACT_STORE_SIZE		(4)
#endif

#define TEXTUREOFFSET(base_,x_,y_) (base_ + (x_ + y_ * GTA_VEHICLE_DAMAGE_TEXTURE_WIDTH))


#define VEHICLE_DEFORMATION_TIMING					(0)

#define TAXI_IDX (21)

#if VEHICLE_DEFORMATION_TIMING
	extern void VD_DisplayTiming();
#endif

#define VEHICLE_LIGHT_COUNT						(VEH_LASTBREAKABLELIGHT - VEH_HEADLIGHT_L + 1 + 1) // +1 for taxi light, +1 for array size.
#define VEHICLE_LIGHT_TAXI						(VEHICLE_LIGHT_COUNT-1)
#define VEHICLE_SIREN_COUNT						(VEH_SIREN_MAX - VEH_SIREN_1 + 1) // + 1 for array size

#define VEHICLEDAMAGE_USE_LINEAR_TEXTURE		(1)
#if VEHICLEDAMAGE_USE_LINEAR_TEXTURE



#else // VEHICLEDAMAGE_USE_LINEAR_TEXTURE
#define DBG 0
#include "xgraphics.h"

	// modified version of XGAddress2DTiledOffset
	inline UINT Fast_XGAddress2DTiledOffset(const UINT x,const UINT y,const UINT Width/*,TexelPitch*/)
	{
		const UINT fixedLog = 2;

		Assert(x < Width);
		Assert(Width == (Width + 31) & ~31);
		Assert(XGLog2LE16(TexelPitch) == fixedLog);

		const UINT Macro = ((x >> 5) + (y >> 5) * (Width >> 5)) << (fixedLog + 7);
		const UINT Micro = (((x & 7) + ((y & 6) << 2)) << fixedLog);
		const UINT Offset = Macro + ((Micro & ~15) << 1) + (Micro & 15) + ((y & 8) << (3 + fixedLog)) + ((y & 1) << 4);

		const UINT result = (((Offset & ~511) << 3) + ((Offset & 448) << 2) + (Offset & 63) + 
			((y & 16) << 7) + (((((y & 8) >> 2) + (x >> 3)) & 3) << 6)) >> fixedLog;
		return result;
	}

#define TEXTUREOFFSET(base_,x_,y_) (base_ + Fast_XGAddress2DTiledOffset(x,y,GTA_VEHICLE_DAMAGE_TEXTURE_SIZE/*,4*/))
	//#define TEXTUREOFFSET(base_,x_,y_) (base_ + XGAddress2DTiledOffset(x,y,GTA_VEHICLE_DAMAGE_TEXTURE_SIZE,4))
#endif // VEHICLEDAMAGE_USE_LINEAR_TEXTURE
// --- Constants ----------------------------------------------------------------

// --- Structure/Class Definitions ----------------------------------------------

	struct VehTexCacheEntry
	{
		CVehicleDeformation *m_pOwner;
		s32 m_nPriority;

#if	GPU_DAMAGE_WRITE_ENABLED && __D3D11
#if VEHICLE_ROLLING_TEXTURE_ARRAY
		fwTexturePayload m_DamageTextures[VEHICLE_DEFORMATION_TEXTURE_ARRAY_SIZE];
		fwTexturePayload m_DamageRenderTarget;
		int m_ActiveCPUIndex;
		int m_ActiveGPUIndex;
		int m_FrameToDownloadOn[VEHICLE_DEFORMATION_TEXTURE_ARRAY_SIZE];
		rage::sysIpcMutex m_DamageMutex;
		bool m_HasDamageDrawn;

		void IncrementQueuedDownload();
		grcTexture *GetDownloadQueuedTexture();
		bool IsDamageQueued();
		bool DamageQueuedIsReady();
		grcTexture *GetDamageDownloadTexture();
		void AdvanceActiveTexture();
		void ClearDamageDownloads();
#else
		fwTexturePayload m_Payloads[2];
#endif
#else
		fwTexturePayload m_Payload;
#endif

	};
	typedef struct VehTexCacheEntry VehTexCacheEntry;

class CVehicleDeformation
{
#if GTA_REPLAY
	friend class CReplayMgr;
#endif

public:
	// damageMap packing constants:
	enum {	YPACK =	(1<<8)		};
	enum {	ZPACK = (1<<16)		};

    static const unsigned NUM_NETWORK_DAMAGE_DIRECTIONS = 6;
    static const unsigned FRONT_LEFT_DAMAGE = 0;
    static const unsigned FRONT_RIGHT_DAMAGE = 1;
    static const unsigned MIDDLE_LEFT_DAMAGE = 2;
    static const unsigned MIDDLE_RIGHT_DAMAGE = 3;
    static const unsigned REAR_LEFT_DAMAGE = 4;
    static const unsigned REAR_RIGHT_DAMAGE = 5;


public:
	CVehicleDeformation();
	~CVehicleDeformation() { /*NoOp*/ };

	void Init(CVehicle* pVehicle);
	void Shutdown();

	// convert from a normalised offset into a texture coordinate (in pixels or UV)
	static Vec::Vector_4V_Out GetDamageTexCoordFromOffset(Vec::Vector_4V_In vecLocalNormalisedOffset, bool inPixels = true);

	// convert from a texture coordinate into a normalised offset
	static Vector3 GetDamageOffsetFromTexCoord(const Vector3& vecTexSampleCoords, bool bCoordsInPixels=true);

#if ADD_PED_SAFE_ZONE_TO_VEHICLES
	ScalarV_Out GetPedSafeAreaMultiplier(Vec3V_In vecOffset) const;
	void GetPedSafeAreaMinMax(Vector3& safeMin, Vector3& safeMax, Vector3& unSafeMin, Vector3& unSafeMax) const;
#endif

	//////////////
	// read and write to damage texture
#if !__DEBUG
	__forceinline
#endif
	static Vec3V_Out ReadFromPixel(const void *basePtr, int x, int y)
	{
		Assert(basePtr && x >= 0 && x<GTA_VEHICLE_DAMAGE_TEXTURE_SIZE && y>=0 && y<GTA_VEHICLE_DAMAGE_TEXTURE_SIZE);

#if GPU_DAMAGE_WRITE_ENABLED
# if VEHICLE_DEFORMATION_USE_HALF_FLOATS
		const TexelValue_A16B16G16R16F* pTexel = TEXTUREOFFSET((const TexelValue_A16B16G16R16F *)basePtr,x,y);
		Vector3 val(Float16Compressor::decompress(pTexel->red), Float16Compressor::decompress(pTexel->green), Float16Compressor::decompress(pTexel->blue));
# else
		const TexelValue_R8G8B8A8_SNORM* pTexel = TEXTUREOFFSET((const TexelValue_R8G8B8A8_SNORM *)basePtr,x,y);
		Vector3 val = Vector3((f32)pTexel->red, (f32)pTexel->green, (f32)pTexel->blue);
		val /= 128.0f;
# endif //VEHICLE_DEFORMATION_USE_HALF_FLOATS
		Vec3V vecBase = VECTOR3_TO_VEC3V(val);
#else
		const float *pFloatEntry = TEXTUREOFFSET((const float *)basePtr,x,y);

		// unpack what's there already
		const ScalarV packedVecBase = ScalarVFromF32(*pFloatEntry);
		Vec3V vecBase = Unpack(packedVecBase);
		
		// scale into -1.0 to 1.0 range
		const Vec3V oneTwentyEight = Vec3V(ScalarVConstant<FLOAT_TO_INT(128.0f)>());
		vecBase -= oneTwentyEight;
		vecBase /= oneTwentyEight;
#endif //GPU_DAMAGE_WRITE_ENABLED

		return vecBase;
	}

	// read from specific pixel
	static Vec3V_Out ReadFromTexPosition(const void *base, Vec2V_In xy, bool bInterpolate);	// round down or interpolate
	Vec3V_Out ReadFromVectorOffset(const void *base, Vec3V_In vecOffset, bool interpolate = true) const;
	static void WriteToPixel(void *base, const Vector3& vecDamage, int x, int y);					// overwrite pixel
	static void AddToPixel(void *base, const Vector3& vecDamage, int x, int y);						// accumulate into pixel
	static void AddToPixel(void *base, Vec3V_In vecDamage, int x, int y);						// accumulate into pixel

	static inline float Pack(int x, int y, int z);
	static inline ScalarV_Out Pack(Vec3V_In xyz);
	static inline void Unpack(Vector3& vecResult);
	static __forceinline Vec3V_Out Unpack(ScalarV_In vecResult);

	/////////////
	// write into areas of the damage texture
	static ScalarV_Out GetTexRange(const Vector3& vecOffset, const float fRadius, bool inPixels);
	void ApplyDamageToCircularArea(void *base,const Vector3& vecDamage, const Vector3& vecOffset, const float fRadius);
	void ResetDamage();

	void ApplyDamageToWindows(const Vector3& vecPos, const float fRadius);

	// called when an impact occurs
	bool ApplyCollisionImpact(const Vector3& vecImpulseWorldSpace, const Vector3& vecPosWorldSpace, CEntity* pOtherEntity, bool bFullScaleDeformation = false, bool bShouldApplyDamageToGlass = true);

	inline float GetNetworkDamage(int idx) { return m_networkDamages[idx]; }

    inline u32 GetNetworkDamageLevel(int idx)
    {
        if(m_networkDamages[idx] <= networkDamageModifiers[idx][0])
            return 0;
        else if(m_networkDamages[idx] <= networkDamageModifiers[idx][1])
            return 1;
        else if(m_networkDamages[idx] <= networkDamageModifiers[idx][2])
            return 2;
        else
            return 3;
    }

    void ApplyDeformationsFromNetwork(u32 damageLevel[NUM_NETWORK_DAMAGE_DIRECTIONS]);
    
#if __BANK
    void ApplyDamageWithOffset(const Vector4 &damage, const Vector4 &offset);
	static void RenderBank();
#endif

	// called from PreRender to copy damage texture across to render target
	void ApplyDeformations(bool bBoundsNeedUpdating = false, bool bNetworkDamageOnly = false);

	void ApplyDamageToBound(const void* basePtr, const fragInst::ComponentBits* pOnlySubBoundGroups = NULL, const Vector3 *pSubBoundGroupsOffset = NULL) const;

	s32 GetImpactStoreCount() const			{ return m_nImpactStoreIdx; }
	void ClearStoredImpacts() { m_nImpactStoreIdx = 0; }
	void SetNewImpactToStore(const Vector4& rDamage, const Vector4& rOffset);
	const Vector4& GetImpactDamage(u32 impactIdx) const { FastAssert(impactIdx < VEHICLE_DEFORMATION_IMPACT_STORE_SIZE); return m_ImpactStore[impactIdx].damage; }
	const Vector4& GetImpactOffset(u32 impactIdx) const { FastAssert(impactIdx < VEHICLE_DEFORMATION_IMPACT_STORE_SIZE); return m_ImpactStore[impactIdx].offset; }

	float GetDamageMultiplier(float* sizeMult = NULL) const;
	float GetDamageMultiplierInv() const { return m_fDamageMultByVehicleSizeInv; }
	void CalculateDamageMultiplier();
	void HandleDamageAdded(void *basePtr, bool bEnableDamageShader, bool bUpdateBounds, bool bUpdateWheels, bool bUpdateAudio, bool bUpdateBones, bool bUpdateGlass);
	void UpdateShaderDamageVars(bool bEnableDamage) { UpdateShaderDamageVars(bEnableDamage, GetBoundRadiusScaled(), GetDamageMultiplier()); }
	void UpdateShaderDamageVars(bool bEnableDamage, float radius, float multiplier);
	inline bool HasDamageTexture() const			{ return(m_nTextureCacheIdx != -1); }
	void DamageTextureAllocate(bool bResetDamage = true);

	void* LockDamageTexture(u32 LockFlags) 
	{
		if ((m_nTextureCacheIdx < 0) || (m_nTextureCacheIdx >= GetTextureCacheSize()))
		{
			return NULL;
		}

		void* ptr = NULL;
		VehTexCacheEntry* entry = &(ms_TextureCache[m_nTextureCacheIdx]);

		DLCPushTimebar("LockDamageTexture");

#if	GPU_DAMAGE_WRITE_ENABLED && __D3D11
#if VEHICLE_ROLLING_TEXTURE_ARRAY
		rage::sysIpcLockMutex(entry->m_DamageMutex);
		ptr = entry->m_DamageTextures[entry->m_ActiveCPUIndex].AcquireBasePtr(LockFlags);
		if (!ptr) rage::sysIpcUnlockMutex(entry->m_DamageMutex);
#else
		ptr = entry->m_Payloads[0].AcquireBasePtr(LockFlags);
#endif
#else
		ptr = entry->m_Payload.AcquireBasePtr(LockFlags);
#endif
		DLCPopTimebar();
		return ptr;
	}

	void UnLockDamageTexture()
	{
		if ((m_nTextureCacheIdx < 0) || (m_nTextureCacheIdx >= GetTextureCacheSize()))
		{
			return;
		}

		VehTexCacheEntry *entry = &(ms_TextureCache[m_nTextureCacheIdx]);

#if	GPU_DAMAGE_WRITE_ENABLED && __D3D11
#if VEHICLE_ROLLING_TEXTURE_ARRAY
		entry->m_DamageTextures[entry->m_ActiveCPUIndex].ReleaseBasePtr();
		rage::sysIpcUnlockMutex(entry->m_DamageMutex);
#else
		entry->m_Payloads[0].ReleaseBasePtr();
#endif
#else
		entry->m_Payload.ReleaseBasePtr();
#endif
	}

#if GPU_DAMAGE_WRITE_ENABLED && __D3D11

	grcRenderTarget* GetDamageRenderTarget() const { return static_cast<grcRenderTarget*>(GetDamageTexture(true)); }
	grcTexture* GetDamageTexture(bool bGetRenderTarget = false) const

#else
	grcRenderTarget* GetDamageRenderTarget() const { return static_cast<grcRenderTarget*>(GetDamageTexture()); }
	grcTexture* GetDamageTexture() const
#endif
	{
		grcTexture *texture = NULL;
		if(HasDamageTexture())
		{
			VehTexCacheEntry *entry = &(ms_TextureCache[m_nTextureCacheIdx]);

#if	GPU_DAMAGE_WRITE_ENABLED && __D3D11
#if VEHICLE_ROLLING_TEXTURE_ARRAY
			if (bGetRenderTarget)
			{
				texture = entry->m_DamageRenderTarget.GetTexture();
			}
			else
			{
				texture = entry->m_DamageTextures[entry->m_ActiveCPUIndex].GetTexture();
			}
#else
			texture = entry->m_Payloads[bGetRenderTarget].GetTexture();
#endif
#else
			texture = entry->m_Payload.GetTexture();
#endif		
		}
		return texture;
	}

	inline fwTexturePayload *GetDamagePayload() const
	{
		fwTexturePayload *payload = NULL;
		if(HasDamageTexture())
		{
#if GPU_DAMAGE_WRITE_ENABLED && __D3D11
#if VEHICLE_ROLLING_TEXTURE_ARRAY
			VehTexCacheEntry &entry = (ms_TextureCache[m_nTextureCacheIdx]);
			payload = &(entry.m_DamageTextures[entry.m_ActiveCPUIndex]);
#else
			payload = &(ms_TextureCache[m_nTextureCacheIdx].m_Payloads[0]);
#endif
#else
			payload = &(ms_TextureCache[m_nTextureCacheIdx].m_Payload);
#endif
		}
		return payload;
	}

	inline float GetBoundRadiusScaledInv() const
	{
		return m_fBoundRadiusScaledInv;
	}

	inline float GetBoundRadiusScaled() const
	{
		return (1.0f / m_fBoundRadiusScaledInv);
	}

	inline ScalarV_Out GetBoundRadiusScaledInvV() const
	{
		return ScalarV(m_fBoundRadiusScaledInv);
	}

	inline ScalarV_Out GetDamageMultiplierScalar() const
	{
		float multiplier = GetDamageMultiplier();
		return ScalarV(multiplier);
	}

#if ADD_PED_SAFE_ZONE_TO_VEHICLES
	CPedSafeZone& GetPedSafeZone() { return m_PedSafeZone; }
	const CPedSafeZone& GetPedSafeZone() const { return m_PedSafeZone; }
#endif

	bool HasValidParentVehicle() { return m_pParentVehicle != NULL; }

private:
	float m_fDamageMultByVehicleSize;
	float m_fDamageMultByVehicleSizeInv;
	float m_fSizeMultiplier;

#if ADD_PED_SAFE_ZONE_TO_VEHICLES
	CPedSafeZone m_PedSafeZone;
#endif

    void UpdateNetworkDamageLevels();

	// --- Structures ------------------------
	struct ImpactRecord
	{
		Vector4 damage; // + fRadius
		Vector4 offset; // + fLength
	};
	typedef struct ImpactRecord ImpactRecord;

	const ImpactRecord& GetImpactStoreIndex(u32 impactIdx) const { FastAssert(impactIdx < VEHICLE_DEFORMATION_IMPACT_STORE_SIZE); return m_ImpactStore[impactIdx]; }

#if VEHICLE_ROLLING_TEXTURE_ARRAY
	public:
		VehTexCacheEntry* GetTexCache() {return &ms_TextureCache[m_nTextureCacheIdx];}

	private:
#endif

	// --- Data ------------------------------

	ImpactRecord m_ImpactStore[VEHICLE_DEFORMATION_IMPACT_STORE_SIZE];

	CVehicle *m_pParentVehicle;

	s32 m_nTextureCacheIdx;
	s32 m_nImpactStoreIdx;

	float m_fBoundRadiusScaledInv;

    // total of impact magnitudes on the vehicles, used for network sync.
    float  m_networkDamages[NUM_NETWORK_DAMAGE_DIRECTIONS];

	// --- Impact record ---------------------
#if VEHICLE_DEFORMATION_TIMING
	s32 m_nImpactFound;
#endif // VEHICLE_DEFORMATION_TIMING
	// --- Texture Cache ---------------------
	static atArray<VehTexCacheEntry>	ms_TextureCache;
	static inline u32				GetTextureCacheSize()					{ return ms_TextureCache.GetCount(); }

	void DamageTextureFree();
	int GetTexCachePriority();

	static void FreeEntry(VehTexCacheEntry &entry);
public:

	static const unsigned MAX_NET_DAMAGE_MODIFIERS = 4;
	static float networkDamageModifiers[NUM_NETWORK_DAMAGE_DIRECTIONS][MAX_NET_DAMAGE_MODIFIERS];

	static grcTexture*      CreateDamageTexture(int width, int height);
	static grcRenderTarget* CreateDamageRenderTarget(grcTexture* texture, int width, int height, int index, bool lockable);

	static void TexCacheInit(unsigned initMode);
	static void TexCacheBuild(bool forMp);
	static void TexCacheUpdate();
	static void TexCacheShutdown(unsigned shutdownMode);
#if __BANK
	static void		DisplayTexCacheStats();
	static void		ClearTexCache();
	static bool		ms_bDisplayTexCacheStats;
	static bool		ms_bShowTextureAndRenderTarget;
	static float	ms_fDisplayTextureScale;
	static bool		ms_bDisplayDamageMap;
	static bool		ms_bDisplayDamageMult;
	static bool		ms_bForceDamageMapScale;
	static bool		ms_bFullDamage;
	static float	ms_fForcedDamageMapScale;
	static float	ms_fWeaponImpactDamageScale;
	static bank_float m_ImpactPositionLocal_X;
	static bank_float m_ImpactPositionLocal_Y;
	static bank_float m_ImpactPositionLocal_Z;
	static bank_float m_Impulse_X;
	static bank_float m_Impulse_Y;
	static bank_float m_Impulse_Z;
	static bank_float m_OffsetDamageToMods_X;
	static bank_float m_OffsetDamageToMods_Y;
	static bank_float m_OffsetDamageToMods_Z;
	static bank_float m_DamageTextureOffset_X;
	static bank_float m_DamageTextureOffset_Y;
	static bank_float m_DamageTextureOffset_Z;
	static bank_float ms_fDamageAmount;
	static bank_float ms_fDamagePercent;
	static bank_float ms_fContainerDropHeight;
	static ScalarV ms_fVehDefColVarMultMin;
	static ScalarV ms_fVehDefColVarMultMax;
	static ScalarV ms_fVehDefColVarAddMin;
	static ScalarV ms_fVehDefColVarAddMax;
	static bool ms_bAutoFix;
	static bool ms_bAutoSaveDamageTexture;
	static bool ms_bUpdateBoundsEnabled;
	static bool ms_bUpdateBonesEnabled;
	static bool		ms_bHidePropellers;
	static float	ms_fForcedPropellerSpeed;
	static void SaveDamageTexture(CVehicle* vehicle, bool skipSaveIfSameAsLast);
	static void LoadDamageTexture(CVehicle* vehicle);
#endif // __BANK

	static float ms_SmoothedPerlinNoise[VEHICLE_DEFORMATION_NOISE_HEIGHT][VEHICLE_DEFORMATION_NOISE_WIDTH_EXPANDED] ;
	static bank_float ms_fVehDefRoofColMultZNeg;
	static bank_float ms_fDeformationCarRoofMinZ;
	static bank_float ms_fDeformationSuperCarRoofMinZ;
	static bank_float ms_fDeformationPlaneHeadMaxZ;
	static bank_float ms_fRollcageMinZ;
	static bank_bool  ms_bEnableRoofZDeformationClamping;

	static bank_float ms_fVehDefColMultX;
	static bank_float ms_fVehDefColMultY;
	static bank_float ms_fVehDefColMultYNeg;
	static bank_float ms_fVehDefColMultZ;
	static bank_float ms_fVehDefColMultZNeg;
	static bank_float ms_fCarModBoneDeformMult;
	static int ms_iExtraExplosionDeformations;
	static bank_float ms_fExtraExplosionsDamage;
	static bool ms_bUpdateDamageFromPhysicsThread;

	static bank_float m_sfVehDefColMax1;
	static bank_float m_sfVehDefColMax2;
	static bank_float m_sfVehDefColMult2;
	static bank_float ms_fLargeVehicleRadiusMultiplier;
	static bank_bool ms_bEnableExtraBoneDeformations;

#if VEHICLE_DEFORMATION_PROPORTIONAL
	static bank_float ms_fDamageMagnitudeMult;
#endif

	static void InitSmoothedPerlinNoise();
	__forceinline static float GetSmoothedPerlinNoiseFloat(int x, int y)
	{
		return ms_SmoothedPerlinNoise[y % VEHICLE_DEFORMATION_NOISE_HEIGHT][x % VEHICLE_DEFORMATION_NOISE_WIDTH];
	}

	__forceinline static Vec4V_Out GetSmoothedPerlinNoise(int x, int y)
	{
		// http://devmag.org.za/2009/04/25/perlin-noise/
		// Wavelength 32 looks appropriate for metal warping damage fluctuations

		const int width  = VEHICLE_DEFORMATION_NOISE_WIDTH;
		const int height = VEHICLE_DEFORMATION_NOISE_HEIGHT;
		CompileTimeAssert(VEHICLE_DEFORMATION_NOISE_WIDTH && !(VEHICLE_DEFORMATION_NOISE_WIDTH & (VEHICLE_DEFORMATION_NOISE_WIDTH - 1)));
		x = x&(width - 1);
		y = rage::Abs(y) % height;

		Vec4V result = Vec4V(Vec::V4LoadUnaligned(&(ms_SmoothedPerlinNoise[y][x])));
		return result;
	}
};

inline float CVehicleDeformation::Pack(int x, int y, int z)
{
	return (float(x) + float(y * YPACK) + float(z * ZPACK));
}

inline ScalarV_Out CVehicleDeformation::Pack(Vec3V_In xyz)
{
	const Vec3V packTerms = Vec3V(1.0f, float(YPACK), float(ZPACK));
	return Dot(xyz, packTerms);
}


inline void CVehicleDeformation::Unpack(Vector3& vecResult)
{
	// unpack z component
	rage::Modf(vecResult.x / float(ZPACK), &vecResult.z);
	vecResult.x -= vecResult.z*float(ZPACK);
	// unpack y component
	rage::Modf(vecResult.x / float(YPACK), &vecResult.y);
	vecResult.x -= vecResult.y*float(YPACK);
}
__forceinline Vec3V_Out CVehicleDeformation::Unpack(ScalarV_In packedValue)
{
	Vec3V unpackedVecResults = Vec3V(packedValue);
	const Vec3V packXYZParams = Vec3V(1.0f, float(YPACK), float(ZPACK));
	const Vec3V unpackXYZParams = Vec3V(1.0f, 1.0f/float(YPACK), 1.0f/float(ZPACK));

	//unpack z component first
	unpackedVecResults.SetZ(RoundToNearestIntZero(unpackedVecResults.GetX() * unpackXYZParams.GetZ()));
	unpackedVecResults.SetX(unpackedVecResults.GetX() - unpackedVecResults.GetZ() * packXYZParams.GetZ());

	//unpack y component
	unpackedVecResults.SetY(RoundToNearestIntZero(unpackedVecResults.GetX() * unpackXYZParams.GetY()));

	unpackedVecResults.SetX(unpackedVecResults.GetX() - unpackedVecResults.GetY() * packXYZParams.GetY());

	return unpackedVecResults;
}


// this enum exists in commands_vehicle.sch as well, so please keep in synch
enum eVehStuckTypes
{
	VEH_STUCK_ON_ROOF = 0,
	VEH_STUCK_ON_SIDE,
	VEH_STUCK_HUNG_UP,
	VEH_STUCK_JAMMED,
	VEH_STUCK_RESET_ALL
};

enum eVehBrokenBouncingState
{
	VEH_BB_NONE = 0,
	VEH_BB_BOUNCING = 1,
	VEH_BB_BROKEN = 2
};

#define MAX_BOUNCING_PANELS		(3)
#define VEH_DAMAGE_HEALTH_STD	(1000.0f)

#define VEH_DAMAGE_PETROL_LEVEL_STD	(65.0f)
#define VEH_DAMAGE_OIL_LEVEL_STD	(5.0f)

#define VEH_DAMAGE_HEALTH_PTANK_LEAKING		(700.0f)
#define VEH_DAMAGE_HEALTH_PTANK_ONFIRE		(0.0f)
#define VEH_DAMAGE_HEALTH_PTANK_FINISHED	(-1000.0f)

#define VEH_DAMAGE_PTANK_LEVEL_BEFOREMISFIRE	(0.1f)

#define VEH_DAMAGE_PTANK_LEAK_RATE	(2.5f)

// force vehicle damage to behave in the same way as before
// (cars go on fire and explode when health goes to zero)
// #define VEHICLE_DAMAGE_FORCE_EXPLOSIONS (1)

class CVehicleDamage
{
public:
	struct Tunables : public CTuning
	{
		Tunables();

		float m_AngleVectorsPointSameDir;
		float m_MinFaultVelocityThreshold;

		// C&C Endurance Damage
		float m_MinImpulseForEnduranceDamage;
		float m_MaxImpulseForEnduranceDamage;
		float m_MinEnduranceDamage;
		float m_MaxEnduranceDamage;
		float m_InstigatorDamageMultiplier;

		// C&C Wanted Status
		float m_MinImpulseForWantedStatus;

		PAR_PARSABLE;
	};

	static Tunables	sm_Tunables;

	CVehicleDamage();
	~CVehicleDamage();

	void Init(CVehicle* pParentVehicle);
	void SetParent(CVehicle* pParent) { m_pParent = pParent; }
	CVehicle* GetParent() {return m_pParent;}

	CVehicleDeformation* GetDeformation() {return &m_Deformation;}
	const CVehicleDeformation* GetDeformation() const {return &m_Deformation;}

	void Process(float fTimeStep, bool bBoundsNeedUpdating = false);
	void PreRender();
	void PreRender2();
	
	float ApplyDamage(CEntity* pInflictor, eDamageType nDamageType, u32 nWeaponHash, float fDamage, const Vector3& vecPos, const Vector3& vecNorm, const Vector3& vecDirn, int nComponent, phMaterialMgr::Id nMaterialId=0, int nPieceIndex=-1, const bool bFireDriveby=false, const bool bIsAccurate = true, const float fDamageRadius = 0.0f, const bool bAvoidExplosions = false, const bool bChainExplosion = false, const bool bMeleeDamage = false, const bool isFlameThrowerFire = false, const bool forceMinDamage = false );
	
	bool HasBoneCollidedWithComponent(eHierarchyId boneId, int iComponent);

	bool CanVehicleBeDamaged(CEntity* pInflictor, u32 nWeaponHash, bool bDoNetworkCloneCheck = true);
	bool CanVehicleBeDamagedBasedOnDriverInvincibility();
	void FixVehicleDamage(bool resetFrag, bool allowNetwork = false);
	void FixVehicleDeformation();

	float GetOverallHealth() const;// {return m_pParent->GetHealth();}
	float GetBodyHealth() const { return m_fBodyHealth; }

    void RefillOilAndPetrolTanks();

	float GetVehicleHealthPercentage(const CVehicle* pVehicle, float maxEngineHealth = 1000.0f, float maxPetrolTankHealth = 1000.0f, float maxHealth = 1000.0f, float maxMainRotorHealth = 1000.0f, float maxRearRotorHealth = 1000.0f, float maxTailBoomHealth = 1000.0f ) const;
	float GetPetrolTankHealth() const {return m_fPetrolTankHealth;}
	bool  GetPetrolTankOnFire() const {return (m_fPetrolTankHealth > VEH_DAMAGE_HEALTH_PTANK_FINISHED && m_fPetrolTankHealth < VEH_DAMAGE_HEALTH_PTANK_ONFIRE);}
	void  ResetPetrolTankHealth() {m_fPetrolTankHealth = VEH_DAMAGE_HEALTH_STD;}
	void  ResetPetrolTankHealthIfItsOverTheMaximum() {if (m_fPetrolTankHealth > VEH_DAMAGE_HEALTH_STD) m_fPetrolTankHealth = VEH_DAMAGE_HEALTH_STD;}
	void  ProcessPetrolTankDamage(float fTimeStep);
	void ProcessFuelConsumption(float fTimeStep);
    void  SetPetrolTankHealth(float fHealth) {Assert(fHealth >= VEH_DAMAGE_HEALTH_PTANK_FINISHED); m_fPetrolTankHealth = fHealth;}
    float GetPetrolTankLevel() const {return m_fPetrolTankLevel;}
    void  SetPetrolTankLevel(float fPetrolLevel);
	bool  IsPetrolTankBurning() const;
	float CalculatePetrolTankFireBurnRate() const;
	float CalculateTimeUntilPetrolTankExplosion() const;

	float GetEngineHealth() const;// {return m_pParent->m_Transmission->GetEngineHealth();}
	bool  GetEngineOnFire() const;
	void  ResetEngineHealth();
	void  SetEngineHealth(float fHealth);
	void  SetBodyHealth(float fHealth);
    void  ProcessOilLeak(float fTimeStep);
    float GetOilLevel() const {return m_fOilLevel;}

	float GetSuspensionHealth(int i) const;
	float GetTyreHealth(int i) const;
	int GetAnyWheelsBurst() const;

	//Finds the closest petrol tank or cap bone in world coords from the local hit position
	void FindClosestPetrolTankPos(const Vector3& vecPosLocal, Vector3* vecPetrolTankTestPos);
	bool GetPetrolTankPosWld(Vector3* pPetrolTankWldPosL, Vector3* pPetrolTankWldPosR=NULL, Vector3* pPetrolTankWldPosCap=NULL);
	bool GetPetrolTankPosLcl(Vector3* pPetrolTankWldPosL, Vector3* pPetrolTankWldPosR=NULL, Vector3* pPetrolTankWldPosCap=NULL);
	const Vector3& GetPetrolSprayPosLocal() { return m_vPetrolSprayPosLocal; }
	const Vector3& GetPetrolSprayNrmLocal() { return m_vPetrolSprayNrmLocal; }

	inline static float GetBodyHealthMax()			{return VEH_DAMAGE_HEALTH_STD;}
	float GetDefaultBodyHealthMax(const CVehicle *pParentVehicle) const;// Get the max body health that is stored in the vehicle model info, if availiable.

	CBouncingPanel* GetBouncingPanel(u32 index) { return index < MAX_BOUNCING_PANELS ? &m_BouncingPanels[index] : NULL; }
	const CBouncingPanel* GetBouncingPanel(u32 index) const { return index < MAX_BOUNCING_PANELS ? &m_BouncingPanels[index] : NULL; }

	// Warning : this take a BONE ID !!! (as in CAR_HEADLIGHT_L)
	inline bool GetLightState(const s32 boneId) const 
	{ 
		Assert((boneId - VEH_HEADLIGHT_L) >= 0);
		Assert((boneId - VEH_HEADLIGHT_L) < VEHICLE_LIGHT_COUNT);
		return m_LightState.IsSet(boneId - VEH_HEADLIGHT_L); 
	}
	// Warning : this take a BONE ID !!! (as in CAR_HEADLIGHT_L)
	inline void SetLightState(const s32 boneId, const bool value)
	{ 
		Assert((boneId - VEH_HEADLIGHT_L) >= 0);
		Assert((boneId - VEH_HEADLIGHT_L) < VEHICLE_LIGHT_COUNT);
		m_LightState.Set(boneId - VEH_HEADLIGHT_L,value); 
	}

	// Warning : this take a 0 to n index
	inline bool GetLightStateImmediate(const s32 idx) const 
	{ 
		Assert(idx>=0);
		Assert(idx<VEHICLE_LIGHT_COUNT);
		return m_LightState.IsSet(idx); 
	}

	inline void SetLightStateImmediate(const s32 idx, const bool value) 
	{ 
		Assert(idx>=0);
		Assert(idx<VEHICLE_LIGHT_COUNT);
		m_LightState.Set(idx,value);
	}
	
	inline bool GetUpdateLightBones(void) const	{ return m_UpdateLightBones;}
	inline void SetUpdateLightBones(const bool value) {m_UpdateLightBones = value;}

	// Warning : this take a BONE ID !!! (as in CAR_HEADLIGHT_L)
	inline bool GetSirenState(const s32 boneId) const 
	{ 
		Assert((boneId - VEH_SIREN_1) >= 0);
		Assert((boneId - VEH_SIREN_1) < VEHICLE_SIREN_COUNT);
		return m_SirenState.IsSet(boneId - VEH_SIREN_1); 
	}
	// Warning : this take a BONE ID !!! (as in CAR_HEADLIGHT_L)
	inline void SetSirenState(const s32 boneId, const bool value)
	{ 
		Assert((boneId - VEH_SIREN_1) >= 0);
		Assert((boneId - VEH_SIREN_1) < VEHICLE_SIREN_COUNT);
		m_SirenState.Set(boneId - VEH_SIREN_1,value); 
	}

	// Warning : this take a 0 to n index
	inline bool GetSirenStateImmediate(const s32 idx) const 
	{ 
		Assert(idx>=0);
		Assert(idx<VEHICLE_SIREN_COUNT);
		return m_SirenState.IsSet(idx); 
	}

	inline void SetSirenStateImmediate(const s32 idx, const bool value) 
	{ 
		Assert(idx>=0);
		Assert(idx<VEHICLE_SIREN_COUNT);
		m_SirenState.Set(idx,value);
	}
	
	inline bool GetUpdateSirenBones(void) const	{ return m_UpdateSirenBones;}
	inline void SetUpdateSirenBones(const bool value) {m_UpdateSirenBones = value;}
	inline int GetSirenHealth(void) const 
	{
		s32 c0unt0 = 0;
		for(int i=0;i<VEHICLE_SIREN_COUNT;i++)
		{
			c0unt0 += (GetSirenStateImmediate(i) == true) ? 1 : 0;
		}
		return c0unt0;
	}
	
	void SetPetrolTankOnFire(CEntity* pEntityResponsible);
	const CEntity* GetEntityThatSetUsOnFire() { return m_pEntityThatSetUsOnFire; }
	void ClearPetrolTankFireCulprit() { m_pEntityThatSetUsOnFire = NULL; }

	void ProcessStuckCheck(float fTimeStep);
	void ResetStuckCheck();
	bool GetIsStuck(int nStuckType, u16 nRequiredTime) const;
    float GetStuckTimer(int nStuckType) const;
	void ResetStuckTimer(int nStuckType);
	bool GetIsDriveable(bool bCheckForPlayerPed = false, bool bIgnoreHealthCheck = false, bool bIgnorePetrolCheck = false) const;

    void ResetBrokenPartCountdown();

	int GetNumOfBrokenOffParts() const;

	int GetNumOfBrokenLoosenParts() const;

	// Car only 
	void BreakOffWheel(int wheelIndex, float ptfxProbability = 1.0f, float deleteProbability = 0.0f, float burstTyreProbability = 0.0f, bool dueToExplosion=false, bool bNetworkCheck = true);
	void BlowUpCarParts(CEntity *pInflictor, int eBreakingState = Break_Off_Car_Parts_Immediately);

	void BreakOffPart(eHierarchyId hierarchyId, int fragChild, fragInst *pFragInst, const CVehicleExplosionInfo* vehicleExplosionInfo, bool bDeleteParts, float fDeleteProb, float fxProb, bool bApplyAdditionalVelocityToPart = true );

	void ApplyAdditionalVelocityToVehiclePart(phInst& oldInst, phInst& newInst, const CVehicleExplosionInfo& vehicleExplosionInfo);
	// Used for bikes, and car and anything else...
	void BlowUpVehicleParts(CEntity* pInflictor);

	void PopOutWindScreen();
    void PopOffRoof(const Vector3 &vRoofImpulse);

	void ApplyDamageToEngine(CEntity* pInflictor, eDamageType nDamageType, float fDamage, const Vector3& vecPosLocal, const Vector3& vecNormLocal, const Vector3& vecDirnLocal, const bool bFireDriveby, const bool bIsAccurate, const float fDamageRadius, const bool bAvoidExplosions = false, u32 nWeaponHash = 0, const bool bChainExplosion = false);
	void UpdateLightsOnBreakOff(int nComponentID);
	
	void BreakOffMiscComponents(float fDeleteMiscChance, float fBreakMiscChance, const Vector3& vecPosLocal);

	u8 GetPartDamage(eHierarchyId ePart);
	void SetPartDamage(eHierarchyId ePart, u8 state);

	static void Copy(CVehicle *dstVehicle, CVehicle* srcVehicle);

	Vector3 RecomputeImpulseFromTrain(CTrain *pTrain, const Vector3& vImpulse) const;

#if __DEV
	// smash test needs to know about these ..
	static const eHierarchyId* GetGlassBoneHierarchyIds();
	static int GetGlassBoneHierarchyIdCount();
#endif // __DEV
#if __BANK
	static void SetupVehicleDoorDamageBank(bkBank& bank);
	static bool ms_bDisplayDamageVectors;
#endif

    static float ms_fArmorModMagnitude;

#if GPU_DAMAGE_WRITE_ENABLED
	static bool ms_bEnableGPUDamage;
	enum eBlowUpCarPartsStates
	{
		Break_Off_Car_Parts_Immediately,
		Break_Off_Car_Parts_Pending_Bound_Update,
		Num_Blow_Up_Car_Parts_States
	};
	u8 m_uBlowUpCarPartsPending;
	bool IsBlowUpCarPartsPending() const {return m_uBlowUpCarPartsPending == Break_Off_Car_Parts_Pending_Bound_Update;}
#endif
#if __BANK

#if GPU_DAMAGE_WRITE_ENABLED
	static int ms_iForcedDamageEveryFrame;
#endif

	static CVehicle *ms_SourceVehicle;
	static CVehicle *ms_DestinationVehicle;
	static u32 g_DamageDebug[CVehicleDeformation::NUM_NETWORK_DAMAGE_DIRECTIONS];
	static float g_DamageValue[CVehicleDeformation::NUM_NETWORK_DAMAGE_DIRECTIONS];
	
	static void SetSourceVehicleCB();
	static void SetDestinationVehicleCB();
	static void DisplayDamageImpulse(const Vector3& impulseDirection, const Vector3& impulsePosition, CVehicle* vehicle, float damageRadius, bool inLocalCoordinates, int framesToLive);
	static void DamageCurrentCar();
	static void DamageCurrentCar(const Vector3& impulseLocal, const Vector3& damagePosLocal, float damage, bool autoFix);
	static void DamageCurrentCarByPercentage();
	static void DamageDriverSideRoof();
	static void HeadOnCollision();
	static void RearEndCollision();
	static void LeftSideCollision();
	static void RightSideCollision();
	static void TailRotorCollision();
	static void ImplodeSubmarine();
	static void RandomSmash();
	static void NetworkSmashDebug();
	static void NetworkSmashDebugRandom();
	static void UpdateNetworkDamageDebug();
	static void RemoveWheel();
	static void RemoveHelicopterTail();
	static void RemoveHelicopterPropellers();
	static void DropFiveTonContainer();
	static void DuplicateDamageCB();
	static void SaveDamageTexture();
	static void LoadDamageTexture();
	static void FixDamageForTests();
	static void BreakOffWheelsCB();

#if VEHICLE_DEFORMATION_PROPORTIONAL
	static void UpdateDamageMultiplier();
#endif
#endif // __BANK

	static void HeadOnCollision(CVehicle* pVehicle, float damage, bool randomize);
	static void RearEndCollision(CVehicle* pVehicle, float damage, bool randomize);

	// PURPOSE:
	//   If this returns true we should try to avoid the results of a vehicle blowing up (explosions, fires, damage, ...) start another
	//     explosion. Since there are so many things that can blow up a vehicle it's not expected that this function is always respected. 
	static bool AvoidVehicleExplosionChainReactions();
	

	static float GetVehicleExplosionBreakChanceMultiplier();

	static void InitTunables(); 

#if __BANK
	static bool ms_bNeverAvoidVehicleExplosionChainReactions;
	static bool ms_bAlwaysAvoidVehicleExplosionChainReactions;

	static bool ms_bUseVehicleExplosionBreakChanceMultiplierOverride;
	static float ms_fVehicleExplosionBreakChanceMultiplierOverride;
#endif // BANK

	static bool ms_bDisableVehiclePartCollisionOnBreak;
	static bool ms_bDisableVehicleExplosionBreakOffParts;

	static float ms_fLooseLatchedDoorOpenAngle;
	static float ms_fLooseLatchedBonnetOpenAngle;
	
	static bank_float sfVehDamPetrolTankApplyGunDamageMult;
	static bank_float sfVehDamPetrolTankApplyGunDamageMultAI;

	static bank_float ms_fChanceToBreak;

	// B*2031517: Value from 0 to 1 that controls how many restrictions are placed on parts breaking of vehicles due to damage.
	// Zero indicates no additional restrictions. One indicates that no parts will break off and VFX will not occur. 
	// Values in between places restrictions on various areas of the vehicles breaking starting with the most superfluous with doors and wheels being the last to be restricted.
	// Restrictions involve increasingly limiting the number of parts breaking off per-vehicle before totally disabling breaking.
	static float ms_fBreakingPartsReduction;

private:
	void ApplyDamageToWheels(CEntity* pInflictor, eDamageType nDamageType, float fDamage, const Vector3& vecPosLocal, const Vector3& vecNormLocal, const Vector3& vecDirnLocal, int nComponent, phMaterialMgr::Id nMaterialId, int nPieceIndex);
	void ApplyDamageToGlass(float fDamage, const Vector3& vecPosLocal, const Vector3& vecNormLocal, const Vector3& vecDirnLocal);
	
	void ApplyDamageToOverallHealth(CEntity* pInflictor, eDamageType nDamageType, u32 nWeaponHash, float fDamage, const Vector3& vecPosLocal, const Vector3& vecDirnLocal, int nComponent, const bool bChainExplosion);
	
	void ApplyDamageToBody(CEntity* pInflictor, eDamageType nDamageType, u32 nWeaponHash, float fDamage, const Vector3& vecPosLocal, const Vector3& vecNormLocal, int nComponent, bool isFlameThrowerFire );

	void ApplyPadShake(const CEntity* pInflictor, float fDamage);
	void ApplyPadShakeInternal();

	void SpewEntityThatSetUsOnFire(const char* DEV_ONLY(text));

public:
	//This updates all vehicle damage trackers, usually it would be private and called withim ApplyDamage() but because of BlowUpCar() I
	//  made this public to fix double network events in some cases.
	void UpdateDamageTrackers(CEntity* pInflictor, u32 nWeaponHash, const eDamageType nDamageType, const float totalDamage, const bool isAlreadyWrecked, const bool isMeleeDamage = false, phMaterialMgr::Id nMaterialId = 0);

	static void DamageVehicleByDriver(const Vector3& impulseLocal, const Vector3& damagePosLocal, float damageAmount, eDamageType nDamageType, CVehicle* vehicle, bool fullDamage);
	static void DamageVehicle(CEntity* pInflictor, const Vector3& impulseLocal, const Vector3& damagePosLocal, float damageAmount, eDamageType nDamageType, CVehicle* vehicle, bool fullDamage);
	static void AddVehicleExplosionDeformations(CVehicle* vehicle, CEntity* pCulprit, eDamageType nDamageType, int numberOfImpulses, float explosionDamageAmount);
	static void DisableVehicleExplosionBreakOffParts()		{ms_bDisableVehicleExplosionBreakOffParts = true;}
	static void ResetVehicleExplosionBreakOffPartsFlag()	{ms_bDisableVehicleExplosionBreakOffParts = false;}

	float &GetDynamicSpoilerDamage() { return m_fDynamicSpoilerDamage; }
	void SetScriptDamageScale( float damageScale ) { m_fScriptDamageScale = damageScale; }
	float GetScriptDamageScale() const { return m_fScriptDamageScale; }

	void SetScriptWeaponDamageScale( float damageScale ) { m_fScriptWeaponDamageScale = damageScale; }
	float GetScriptWeaponDamageScale() const { return m_fScriptWeaponDamageScale; } 

	void SetDisableDamageWithPickedUpEntity( bool disableDamage ) { m_bDisableDamageWithPickedUpEntity = disableDamage; }

	void SetDamageScales( float bodyDamageScale, float petrolTankDamageScale, float engineDamageScale );
	void SetHeliDamageScales( float mainRotorDamageScale, float rearRotorDamageScale, float tailBoomDamageScale );

	float GetBodyDamageScale() const { return m_fBodyDamageScale; }
	void SetBodyDamageScale( float damageScale ) { m_fBodyDamageScale = damageScale; }
	
	float GetPetrolTankDamageScale() const { return m_fPetrolTankDamageScale; }
	void SetPetrolTankDamageScale( float damageScale ) { m_fPetrolTankDamageScale = damageScale; }

	float GetCollisionWithMapDamageScale() const { return m_fCollisionWithMapDamageScale; }
	void SetCollisionWithMapDamageScale( float damageScale ) { m_fCollisionWithMapDamageScale = damageScale; }

private:
	//Updates network stuff related to damage tracking like triggering the network events. Its called by UpdateDamageTrackers().
	void UpdateNetworkDamageTracker(CEntity* pInflictor, const float fDamage, const u32 uWeaponHash, const bool vehicleIsDestroyed, const bool isMeleeDamage = false, phMaterialMgr::Id nMaterialId = 0) const;

	void  ResetFragCacheEntry();
    void ApplyDamageToBreakablePanels( eDamageType nDamageType, float fDamage, const Vector3& vecPosLocal, int nComponent );

private:
	CVehicleDeformation m_Deformation;
	
	// array of bouncing panels - stores 
	CBouncingPanel m_BouncingPanels[MAX_BOUNCING_PANELS];

	Vector3 m_vPetrolSprayPosLocal;
	Vector3 m_vPetrolSprayNrmLocal;
	static float ms_fPetrolTankFireBurnRateMin;
	static float ms_fPetrolTankFireBurnRateMax;

	Vector3 m_vecOldMoveSpeed;
	Vector3 m_vecOldTurnSpeed;

	Vector3 m_vecLastStuckPos;
	Vector3 m_vecLastStuckJammedPos;
	u16 m_nStuckCounterOnRoof;
	u16 m_nStuckCounterOnSide;
	u16 m_nStuckCounterHungUp;
	u16 m_nStuckCounterJammed;

	CVehicle* m_pParent;
	float m_fBodyHealth;
	float m_fPetrolTankHealth;
	float m_fBodyDamageScale;
	float m_fPetrolTankDamageScale;
	float m_fCollisionWithMapDamageScale;

    float m_fPetrolTankLevel;
    float m_fOilLevel;

	RegdEnt m_pEntityThatSetUsOnFire;

	f32 m_fCountDownToTimeAnotherPartCanBreakOff;

	static u32 sm_TimeOfLastShotAtHeliMegaphone;

	atFixedBitSet<VEHICLE_LIGHT_COUNT>	m_LightState;
	atFixedBitSet<VEHICLE_SIREN_COUNT>	m_SirenState;

	bool								m_UpdateLightBones;
	bool								m_UpdateSirenBones;
	 
	bool m_bHasHandBrake;
	bool m_bDisableDamageWithPickedUpEntity;

	float m_fPadShakeIntensity;
	u8 m_uPadShakeDuration;

	float m_fDynamicSpoilerDamage;
	float m_fScriptDamageScale;
	float m_fScriptWeaponDamageScale;

	// Cloud tunables
	static bool sbPlaneDamageCapLargePlanesOnly;
	static float sfPlaneExplosionDamageCapInMP;
	static float sfSpecialAmmoFMJDamageMultiplierAgainstVehicles;
	static float sfSpecialAmmoFMJDamageMultiplierAgainstVehicleGasTank;
};


#endif // !INC_VEHICLEDAMAGE_H_

