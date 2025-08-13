//
// filename:	EntitySelect.cpp
// description:	
//

// --- Include Files ------------------------------------------------------------
#include "EntitySelect.h"

#if ENTITYSELECT_ENABLED_BUILD

// RAGE headers
#include "profile/timebars.h"
#include "grcore/allocscope.h"
#include "grcore/device.h"
#include "grcore/im.h"
#include "grcore/texture.h"
#include "grcore/viewport.h"
#include "grmodel/shaderfx.h"
#include "bank/bank.h"
#include "input/mouse.h"
#include "fwscene/world/SceneGraphNode.h"
#include "fwrenderer/renderthread.h"
#include "system/param.h"
#include "system/xtl.h"
#include "math/vecrand.h"

//For bank
#include "atl/array.h"

#if __PPU
#	include "grcore/wrapper_gcm.h"
#endif

#if __D3D
#	include "system/d3d9.h"
#	if __XENON
#		include "system/xtl.h"
#		define DBG 0			// yuck
#		include <xgraphics.h>
#	endif
#endif

// Game headers
#include "scene/Building.h"
#include "scene/AnimatedBuilding.h"
#include "Objects/DummyObject.h"
#include "Objects/object.h"
#include "Peds/ped.h"
#include "pickups/Pickup.h"
#include "Vehicles/vehicle.h"
//Debug Rendering Support
#include "Renderer/Util/ShaderUtil.h"
#include "Renderer/sprite2d.h"

#if __XENON
#include "grcore/texturexenon.h"
#endif

RENDER_OPTIMISATIONS()

PARAM(EntitySelectTargetSize, "Entity Select Render Target Size for Debugging");

#if 1 // normal usage, made it slightly bigger for visualization purposes
	#define ENTITYSELECT_DISPLAY_SIZE 64
	#define ENTITYSELECT_TARGET_SIZE  64 //Must be at least 32 because of 32-pixel alignment requirement. (on Xenon, for resolve)
#else // debugging, do not s-u-b-m-i-t
	#define ENTITYSELECT_DISPLAY_SIZE 256
	#define ENTITYSELECT_TARGET_SIZE  256
#endif

// --- Constants ----------------------------------------------------------------
static const char* g_PedShaderNames[] =
{
	"ped",
	"ped_alpha",
	"ped_cloth",
	"ped_cloth_enveff",
	"ped_decal",
	"ped_decal_decoration",
	"ped_decal_exp",
	"ped_decal_medals",
	"ped_decal_nodiff",
	"ped_default",
	"ped_default_cloth",
	"ped_default_enveff",
	"ped_default_mp",
	"ped_default_palette",
	"ped_emissive",
	"ped_enveff",
	"ped_fur",
	"ped_hair_cutout_alpha",
	"ped_hair_cutout_alpha_cloth",
	"ped_hair_spiked",
	"ped_hair_spiked_enveff",
	"ped_hair_spiked_mask",
	"ped_nopeddamagedecals",
	"ped_palette",
	"ped_wrinkle",
	"ped_wrinkle_cloth_enveff",
	"ped_wrinkle_cloth",
	"ped_wrinkle_cs",
	"ped_wrinkle_enveff"
};

namespace ES_Global
{
	BankBool sScrambleBits = true;
}

// --- Structure/Class Definitions ----------------------------------------------
class CEntitySelectDebugDraw
{
public:
	void PreDebugRender(grcRenderTarget *texture, bool filtering, grcBlendStateHandle blendState, grcDepthStencilStateHandle depthState)
	{
		if(!texture)
			return;

		CSprite2d::CustomShaderType type = CSprite2d::CS_BLIT;
		switch(texture->GetType())
		{
		case grcrtDepthBuffer:
		case grcrtShadowMap:
			type = CSprite2d::CS_BLIT_DEPTH;
			break;

		default:
			type = (filtering ? CSprite2d::CS_BLIT : CSprite2d::CS_BLIT_NOFILTERING);
			break;
		}

		static const Vector4 sBlitColorMultParams(1.0f, 1.0f, 1.0f, 1.0f);
		mBlit.SetTexture(texture);
		mBlit.SetRenderState();
		mBlit.SetGeneralParams(sBlitColorMultParams, Vector4(0.0f, 0.0, 0.0, 0.0));

		grcStateBlock::SetBlendState(blendState);
		grcStateBlock::SetDepthStencilState(depthState);

		mBlit.BeginCustomList(type, texture);
	}

	void PostDebugRender()
	{
		mBlit.EndCustomList();
		grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
	}

	static void DrawFullscreen()
	{
		DrawFullscreen(Vector2(0.0f, 0.0f), Vector2(1.0f, 1.0f));
	}

	static void DrawFullscreen(const Vector2 &uv1, const Vector2 &uv2)
	{
		grcDrawSingleQuadf(-1.0f, 1.0f, 1.0f, -1.0f, 0.0f, uv1.x, uv1.y, uv2.x, uv2.y, Color32(255,255,255,255));
	}

	static void DrawScreenQuad(const Vector2 &pos, const Vector2 &size)
	{
		const float sx = 1.0f / (float)GRCDEVICE.GetWidth();
		const float sy = 1.0f / (float)GRCDEVICE.GetHeight();

		float x1 = pos.x;
		float y1 = GRCDEVICE.GetHeight() - pos.y;
		float x2 = (pos.x + size.x);
		float y2 = GRCDEVICE.GetHeight() - (pos.y + size.y);

		Vector4 normCoords(x1 * sx, y1 * sy, x2 * sx, y2 * sy); //Transform to [0, 1] space.
		normCoords = (normCoords - Vector4(0.5f, 0.5f, 0.5f, 0.5f)) * Vector4(2.0f, 2.0f, 2.0f, 2.0f); //Transform to [-1, 1] space.

		grcDrawSingleQuadf(normCoords.x, normCoords.y, normCoords.z, normCoords.w, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, Color32(255,255,255,255));
	}

private:
	CSprite2d mBlit;
};

//Simple class to help manage rendertargets. (Mostly for freeing... should not release passed in targets, but should release self-managed ones.)
class CEntitySelectRenderTarget
{
public:
	typedef CEntitySelectRenderTarget this_type;
	typedef grcRenderTarget value_type;
	typedef grcRenderTarget * pointer;
	typedef grcRenderTarget & reference;
	typedef const grcRenderTarget & const_reference;

	CEntitySelectRenderTarget()
	: mSelfManaged(false)
	{
		for(int i=0; i<ENTITYSELECT_NO_OF_PLANES; i++)
			mRT[i] = NULL;
	}

	//Copy constructor must be defined because a copy of this class becomes an alias to a render target, so self managed needs to be false.
	CEntitySelectRenderTarget(const this_type &cp)
	: mSelfManaged(false)
	{
		for(int i=0; i<ENTITYSELECT_NO_OF_PLANES; i++)
			mRT[i] = cp.mRT[i];

	}

	~CEntitySelectRenderTarget() { Release(); }

	//Create a target. This creates a render target that will be managed by this class. So when release is called, it will actually release the rendertarget. 
	inline void Create(const char *name, grcRenderTargetType type, int width, int height, int bitsPerPixel, grcTextureFactory::CreateParams *params = NULL)
	{
		Release();

		mSelfManaged = true;
		for(int i=0; i<ENTITYSELECT_NO_OF_PLANES; i++)
			mRT[i] = grcTextureFactory::GetInstance().CreateRenderTarget(name, type, width, height, bitsPerPixel, params);
	}

	//Release the current render target. If this target is self managed (created by calling create) then the target will actually be released.
	//If this is not self managed (created by using the assignment operator) we will just release our reference to the target. The caller will
	//be responsible for managing the actual lifetime of the render target.
	inline void Release()
	{
		if(mSelfManaged)
		{
			for(int i=0; i<ENTITYSELECT_NO_OF_PLANES; i++)
			{
				if(mRT[i] != NULL)
					mRT[i]->Release();
				mRT[i] = NULL;
			}
		}
		else
		{
			for(int i=0; i<ENTITYSELECT_NO_OF_PLANES; i++)
				mRT[i] = NULL;
		}

		mSelfManaged = false;
	}

#if ENTITYSELECT_NO_OF_PLANES == 1
	//Assignment Operator - This is used to alias a render target. When an alias is released, this class will simply NULL out the rt pointer.
	//(Caller is responsible for managing the target's lifetime)
	inline this_type &operator =(grcRenderTarget *rt)	{ mRT[0] = rt; mSelfManaged = false; return *this; }
#endif // ENTITYSELECT_NO_OF_PLANES == 1

	bool LockRect(int layer, int mipLevel, int flags) 
	{ 
		memset(&mTexLocks, 0, sizeof(grcTextureLock)*ENTITYSELECT_NO_OF_PLANES);

		for(int i=0; i<ENTITYSELECT_NO_OF_PLANES; i++)
		{
			if(mRT[i]->LockRect(layer, mipLevel, mTexLocks[i], flags) == false)
			{
				Assert(mTexLocks[i].BitsPerPixel == 32);
				Assert(mTexLocks[i].Width == GetWidth());
				Assert(mTexLocks[i].Height == GetHeight());
				return false;
			}
		}
		return true; 
	}
	void UnlockRect() 
	{
		for(int i=0; i<ENTITYSELECT_NO_OF_PLANES; i++)
			if(mTexLocks[i].Base != NULL)
				mRT[i]->UnlockRect(mTexLocks[i]);
	};

	// Reads and unscrambles.
	entitySelectID Read(int x, int y)
	{
		entitySelectID ret = 0;

		for(int i=0; i<ENTITYSELECT_NO_OF_PLANES; i++)
		{
			AssertMsg(mTexLocks[i].Base, "CEntitySelectRenderTarget::Read()...Plane not locked!" );
			u32 rtVal = ((u32 *)mTexLocks[i].Base)[x + mTexLocks[i].Width*y];
			if(ES_Global::sScrambleBits)
				rtVal = UnscrambleBits32(rtVal);
			ret |= ((entitySelectID)rtVal & 0xffffffff) << (i *32);
		}
		return ret;
	}
	// Writes directly.
	void Write(int x, int y, entitySelectID val)
	{
		for(int i=0; i<ENTITYSELECT_NO_OF_PLANES; i++)
		{
			AssertMsg(mTexLocks[i].Base, "CEntitySelectRenderTarget::Write()...Plane not locked!" );
			u32 rtVal = (u32)(val >> (i*32));
			((u32 *)mTexLocks[i].Base)[x + mTexLocks[i].Width*y] = rtVal;
		}
	}

	int GetWidth() { return mRT[0]->GetWidth(); }
	int GetHeight() { return mRT[0]->GetHeight(); }
	grcRenderTarget *GetRT(int i) { return mRT[i]; };
	const grcRenderTarget *GetRT(int i) const { return mRT[i]; };

	bool IsValid() 
	{ 
		for(int i=0; i<ENTITYSELECT_NO_OF_PLANES; i++)
			if(mRT[i] == NULL)
				return false;
		return true;
	}

private:
	bool mSelfManaged;
	grcRenderTarget *mRT[ENTITYSELECT_NO_OF_PLANES];
	grcTextureLock mTexLocks[ENTITYSELECT_NO_OF_PLANES];
};

// --- Globals ------------------------------------------------------------------

// --- Static Globals ----------------------------------------------------------
namespace ES_Global
{
	//Global shader Vars
	grcEffectGlobalVar sEntitySelectVarID = grcegvNONE;

	//Global Technique Vars
	DECLARE_MTR_THREAD s32	sPrevTechniqueGroupId = -1;
	s32	sESTechniqueGroupId = -1;

	//Render Target Variables
	int sBackBufferWidth = 0;
	int sBackBufferHeight = 0;
	u32 sRtWidth = ENTITYSELECT_TARGET_SIZE;
	u32 sRtHeight = ENTITYSELECT_TARGET_SIZE;

	int sResolveMaxX = sBackBufferWidth;
	int sResolveMaxY = sBackBufferHeight;

	CEntitySelectRenderTarget sEntitySelectRT;
	CEntitySelectRenderTarget sEntitySelectDepth;
	CEntitySelectRenderTarget sEntitySelectResolve[2];

	//Window Space rect
	struct windowRect {
		u32 X1;
		u32 Y1;
		u32 X2;
		u32 Y2;
		};
	windowRect sWindowRects[2] = { {0U, 0U, sRtWidth, sRtHeight}, {0U, 0U, sRtWidth, sRtHeight} };

	Vector2 sMouseCoords = Vector2(0.0f, 0.0f);
	int sPointerPixelX = 0;
	int sPointerPixelY = 0;

	//Render States

	DECLARE_MTR_THREAD bool sIsEntitySelectPassCurrent = false;
	DECLARE_MTR_THREAD bool sIsEntitySelectExternalPassCurrent = false; // not the actual entity select pass, but still need to set the entity id shader var

	//Sampled id
	entitySelectID sSampledEntityId = CEntityIDHelper::GetInvalidEntityId();
	
	//Debug Drawing
	bool sVisualizeEntitySelectRT = false;
	int sVisualizeEntitySelectPlane = 0;
	bool sVisualizeFullscreen = false;
	bool sVisualizeCenterOnMouse = true; // this is more useful being enabled by default i think
	bool sVisualizeOverrideSize = true;

	int sVisualizePosX = 64;
	int sVisualizePosY = 32;
	int sVisualizeWidth = ENTITYSELECT_DISPLAY_SIZE;
	int sVisualizeHeight = ENTITYSELECT_DISPLAY_SIZE;

	grcBlendStateHandle rgbWriteBS;

	//Debug render state controls
	bool sSetScissor = !RSG_PC; // Should actually work on PC, I'd say.

	////////////////////////////////////////////////
	// Initialization Helper Functions
	//
	void InitializeFromParams()
	{
		u32 paramSize = 0;
		if(PARAM_EntitySelectTargetSize.Get(paramSize) && paramSize > ENTITYSELECT_TARGET_SIZE)
		{
			ES_Global::sRtWidth = ES_Global::sRtHeight = 
			sWindowRects[0].X2 = sWindowRects[0].Y2 = 
			sWindowRects[1].X2 = sWindowRects[1].Y2 = paramSize;
			sVisualizeWidth = sVisualizeHeight = static_cast<int>(paramSize);
		}
	}

	void ComputeBackBufferDimentions()
	{
		sBackBufferWidth = grcTextureFactory::GetInstance().GetBackBuffer(false)->GetWidth();
		sBackBufferHeight = grcTextureFactory::GetInstance().GetBackBuffer(false)->GetHeight();

		sResolveMaxX = Max<int>(sBackBufferWidth - sRtWidth, 0);
		sResolveMaxY = Max<int>(sBackBufferHeight - sRtHeight, 0);
	}

	bool IsInitialized()
	{
		return (sEntitySelectDepth.IsValid() && sEntitySelectRT.IsValid() && sEntitySelectResolve[0].IsValid() && sEntitySelectResolve[1].IsValid() && sEntitySelectVarID != grcegvNONE && sESTechniqueGroupId != -1);
	}

	////////////////////////////////////////////////
	// Rendering Helper Functions
	//

	void SetESForcedTechnique()
	{
		sPrevTechniqueGroupId = grmShaderFx::GetForcedTechniqueGroupId();
		grmShaderFx::SetForcedTechniqueGroupId(sESTechniqueGroupId);
	}

	void ClearESForcedTechnique()
	{
		//Restore previously forced technique
		grmShaderFx::SetForcedTechniqueGroupId(sPrevTechniqueGroupId);
	}

	struct ESIdParam
	{
		Vec4V m_Planes[ENTITYSELECT_NO_OF_PLANES];
	};

	ESIdParam ComputeESIdParamVal(entitySelectID id)
	{
		ESIdParam ret;

		for(int i=0; i<ENTITYSELECT_NO_OF_PLANES; i++)
		{
			u32 id_plane = (u32)(id >> (i *32));
			if(ES_Global::sScrambleBits)
			{
				Assertf(UnscrambleBits32(ScrambleBits32(id_plane)) == id_plane, "WARNING: Bit scramble does not properly unscramble 0x%8x. [Scrambled = 0x%8x | Unscrambled = 0x%8x]",
						id_plane, ScrambleBits32(id_plane), UnscrambleBits32(ScrambleBits32(id_plane)));
				id_plane = ScrambleBits32(id_plane);
			}
			Color32 idCol(id_plane);
			ret.m_Planes[i] = idCol.GetRGBA();
		}
		return ret;
	}

	inline void SetESIdShaderVars(const ESIdParam &param, bool isValid)
	{
		if(isValid)
		{
			grcEffect::SetGlobalVar(sEntitySelectVarID, &param.m_Planes[0], ENTITYSELECT_NO_OF_PLANES);
		}
		else
		{
			ESIdParam sInvalidID;

			for(int i=0; i<ENTITYSELECT_NO_OF_PLANES; i++)
				sInvalidID.m_Planes[i] = Vec4V(-1.0f, -1.0f, -1.0f, -1.0f);

			grcEffect::SetGlobalVar(sEntitySelectVarID, &sInvalidID.m_Planes[0], ENTITYSELECT_NO_OF_PLANES);
		}
	}

	entitySelectID SampleCurrentEntityId()
	{
		static const int sLockLayer = 0;
		static const int sLockMipLevel = 0;
		static const u32 sLockFlags = grcsRead; // | grcsAllowVRAMLock;
		entitySelectID selectedEntityId = CEntityIDHelper::GetInvalidEntityId();

		// Actually, we're accessing the update buffer, but because we can only lock on the renderthread, we lie and use the 1 - renderid
		int bufferId = 1 - gRenderThreadInterface.GetRenderBuffer();
		
		if(Verifyf(sEntitySelectResolve[bufferId].LockRect(sLockLayer, sLockMipLevel, sLockFlags), "Warning: Unable to lock Entity Select target to read current value!"))
		{
			int pointerOffsetX = Clamp(sPointerPixelX - static_cast<int>(sWindowRects[bufferId].X1) + (int)sRtWidth /2 - 16, 0, sEntitySelectResolve[bufferId].GetWidth()  - 1); //col
			int pointerOffsetY = Clamp(sPointerPixelY - static_cast<int>(sWindowRects[bufferId].Y1) + (int)sRtHeight/2 - 16, 0, sEntitySelectResolve[bufferId].GetHeight() - 1); //row

			selectedEntityId = sEntitySelectResolve[bufferId].Read(pointerOffsetX, pointerOffsetY);
			// Done with texture, unlock it.
			sEntitySelectResolve[bufferId].UnlockRect();
		}
		return selectedEntityId;
	}

#if __XENON
	void ResolveTargets()
	{
		Assert(IsInitialized());

		int bufferId = gRenderThreadInterface.GetRenderBuffer();
		
		D3DRECT mousePointerRect;
		mousePointerRect.x1 = sWindowRects[bufferId].X1;	mousePointerRect.y1 = sWindowRects[bufferId].Y1;
		mousePointerRect.x2 = sWindowRects[bufferId].X2;	mousePointerRect.y2 = sWindowRects[bufferId].Y2;

		if(D3DTexture *destTex = static_cast<D3DTexture *>(sEntitySelectResolve[bufferId]->GetTexturePtr()))
			GRCDEVICE.GetCurrent()->Resolve(D3DRESOLVE_RENDERTARGET0,
											&mousePointerRect,
											destTex,
											NULL, 0, 0,
											NULL,
											0.0f,
											0L,
											NULL);
	}
#else
	GRC_ALLOC_SCOPE_DECLARE_GLOBAL(static, s_ES_Global_AllocScope)


	void CopyESRT()
	{
		Assert(IsInitialized());

		GRC_ALLOC_SCOPE_PUSH(s_ES_Global_AllocScope)

		int bufferId = gRenderThreadInterface.GetRenderBuffer();
		
		//Can't do an actual resolve on other platforms. Instead, we just execute a copy render pass.
		//Note: We could also just sample the large render target directly... but will copy for now b/c 1- gbuff targets are tiled, 2- support debug visualization.
		float width  = static_cast<float>(sEntitySelectRT.GetWidth());
		float height = static_cast<float>(sEntitySelectRT.GetHeight());
		Vector2 uv1(static_cast<float>(sWindowRects[bufferId].X1) / width,	static_cast<float>(sWindowRects[bufferId].Y1) / height);
		Vector2 uv2(static_cast<float>(sWindowRects[bufferId].X2) / width,	static_cast<float>(sWindowRects[bufferId].Y2) / height);
		CEntitySelectDebugDraw renderer;

		for(int i=0; i<ENTITYSELECT_NO_OF_PLANES; i++)
		{
			grcTextureFactory::GetInstance().LockRenderTarget(0, sEntitySelectResolve[bufferId].GetRT(i), NULL);	//This must be done after unlock of main Render Target.
			renderer.PreDebugRender(sEntitySelectRT.GetRT(i), false, grcStateBlock::BS_Default, grcStateBlock::DSS_IgnoreDepth);
			renderer.DrawFullscreen(uv1, uv2);
			renderer.PostDebugRender();
			grcTextureFactory::GetInstance().UnlockRenderTarget(0);
		}

		GRC_ALLOC_SCOPE_POP(s_ES_Global_AllocScope);
	}

	void InitializeResolveTarget(u32 initialValue = CEntityIDHelper::GetInvalidEntityId())
	{
		static const int sLockLayer = 0;
		static const int sLockMipLevel = 0;
		static const u32 sLockFlags = grcsWrite;

		for(int i=0;i<2;i++)
		{
			if(Verifyf(sEntitySelectResolve[i].LockRect(sLockLayer, sLockMipLevel, sLockFlags), "Warning: Unable to lock and initialize Entity Select resolve target!"))
			{
				for(int row = 0; row < sEntitySelectResolve[i].GetWidth(); ++row)
				{
					for(int col = 0; col < sEntitySelectResolve[i].GetHeight(); ++col)
					{
						sEntitySelectResolve[i].Write(col, row, initialValue);
					}
				}

				// Done with texture, unlock it.
				sEntitySelectResolve[i].UnlockRect();
			}
		}
	}
#endif

	void ClearResolveTargets()
	{
		// This is ok as all planes will be 0xffffffff.
		Color32 invalidId = Color32((u32)CEntityIDHelper::GetInvalidEntityId());

		for(int i=0; i<2; i++)
		{
			for(int trgt=0; trgt<ENTITYSELECT_NO_OF_PLANES; trgt++)
			{
				grcTextureFactory::GetInstance().LockRenderTarget(0, sEntitySelectResolve[i].GetRT(trgt), NULL);
				GRCDEVICE.Clear(true, invalidId, false, 0.0f, false, 0);
				grcTextureFactory::GetInstance().UnlockRenderTarget(0, NULL);
			}
		}
	}
}

// --- Static class members -----------------------------------------------------
bool CEntitySelect::sEntitySelectRenderPassEnabled = false; // this must be false while the game is loading
bool CEntitySelect::sEntitySelectUsingShaderIndexExtension = true;
bool CEntitySelect::sEntitySelectEnablePedShaderHack = true;

// --- Code ---------------------------------------------------------------------

//--------------------------------------------------------------------

void CEntitySelect::Update()
{
	//NOTE:	Resolve rect must be (8x8) aligned (GPU_RESOLVE_ALIGNMENT) according to docs...
	//		BUT... in actuality, must be 32 pixel aligned! (Because pDestPoint needs to be 0,0)
	Assert(ES_Global::sResolveMaxX == Max<int>(ES_Global::sBackBufferWidth  - ES_Global::sRtWidth,  0));
	Assert(ES_Global::sResolveMaxY == Max<int>(ES_Global::sBackBufferHeight - ES_Global::sRtHeight, 0));

	ES_Global::sMouseCoords.Set(Saturate(ioMouse::GetNormX()), Saturate(ioMouse::GetNormY()));
	ES_Global::sPointerPixelX = static_cast<int>(ES_Global::sMouseCoords.x * static_cast<float>(ES_Global::sBackBufferWidth))  - (ES_Global::sRtWidth /2 - 16);
	ES_Global::sPointerPixelY = static_cast<int>(ES_Global::sMouseCoords.y * static_cast<float>(ES_Global::sBackBufferHeight)) - (ES_Global::sRtHeight/2 - 16);

	int bufferId = gRenderThreadInterface.GetUpdateBuffer();

	ES_Global::sWindowRects[bufferId].X1 = Clamp(ES_Global::sPointerPixelX, 0, ES_Global::sResolveMaxX - 1) & 0xFFFFFFE0;	//Mask to make 32-pixel aligned
	ES_Global::sWindowRects[bufferId].Y1 = Clamp(ES_Global::sPointerPixelY, 0, ES_Global::sResolveMaxY - 1) & 0xFFFFFFE0;
	ES_Global::sWindowRects[bufferId].X2 = Min<u32>(ES_Global::sWindowRects[bufferId].X1 + ES_Global::sRtWidth,  ES_Global::sBackBufferWidth);
	ES_Global::sWindowRects[bufferId].Y2 = Min<u32>(ES_Global::sWindowRects[bufferId].Y1 + ES_Global::sRtHeight, ES_Global::sBackBufferHeight);
}

#if ENTITYSELECT_NO_OF_PLANES == 1

void CEntitySelect::Init(grcRenderTarget* fullsizeRGBTarget, grcRenderTarget* fullsizeDepthTarget)

#else

void CEntitySelect::Init(grcRenderTarget* ASSERT_ONLY(fullsizeRGBTarget), grcRenderTarget* ASSERT_ONLY(fullsizeDepthTarget))

#endif
{
#if ENTITYSELECT_NO_OF_PLANES != 1
	AssertMsg(fullsizeRGBTarget == NULL, "Can`t use custom targets for Entity select > 32 bits.");
	AssertMsg(fullsizeDepthTarget == NULL, "Can`t use custom targets for Entity select > 32 bits.");
#endif //ENTITYSELECT_NO_OF_PLANES != 1

	ES_Global::InitializeFromParams();
	ES_Global::sESTechniqueGroupId = grmShaderFx::FindTechniqueGroupId("entityselect");	
	
	ES_Global::sEntitySelectVarID = grcEffect::LookupGlobalVar("EntitySelectColor");

	grcBlendStateDesc BSDesc;
	BSDesc.BlendRTDesc[0].RenderTargetWriteMask= grcRSV::COLORWRITEENABLE_RGB;
	ES_Global::rgbWriteBS = grcStateBlock::CreateBlendState(BSDesc);
		
	ES_Global::sEntitySelectResolve[0].Release();
	ES_Global::sEntitySelectResolve[1].Release();
	ES_Global::sEntitySelectRT.Release();
	ES_Global::sEntitySelectDepth.Release();

	static const int sBPP = 32;

	//Get backbuffer info
	ES_Global::ComputeBackBufferDimentions();

#if ENTITYSELECT_NO_OF_PLANES == 1
	bool createFullsizeRt = true;
	bool createFullsizeDepth = true;

	if(fullsizeRGBTarget)
	{
		const grcRenderTargetType type = fullsizeRGBTarget->GetType();
		//No common way to get back the grcTextureFormat??
		grcTextureFormat format = static_cast<grcTextureFormat>(fullsizeRGBTarget->GetSurfaceFormat());
		XENON_ONLY(if(grcRenderTargetXenon *xTex = dynamic_cast<grcRenderTargetXenon *>(fullsizeRGBTarget)) format = xTex->GetFormat());
		PS3_ONLY(format = ((static_cast<CellGcmEnum>(format) == CELL_GCM_SURFACE_A8B8G8R8) ? grctfA8R8G8B8 : grctfNone));
		D3D11_ONLY(if(grcRenderTargetD3D11 *xTex = dynamic_cast<grcRenderTargetD3D11 *>(fullsizeRGBTarget)) format = xTex->GetFormat());

		if(	Verifyf(type == grcrtPermanent || type == grcrtFrontBuffer || type == grcrtBackBuffer, "Supplied rgb render target is not the proper type! Type = %d", type) &&
			Verifyf(format == grctfA8R8G8B8, "Supplied rgb render target is not the proper format! Format must be grctfA8R8G8B8. (Format = %d)", format) )
			createFullsizeRt = false;
	}

	if(fullsizeDepthTarget)
	{
		const grcRenderTargetType type = fullsizeDepthTarget->GetType();
		if(Verifyf(type == grcrtDepthBuffer, "Supplied depth render target is not the proper type! Type must be grcrtDepthBuffer. (Type = %d)", type))
			createFullsizeDepth = false;
	}
#endif // ENTITYSELECT_NO_OF_PLANES == 1

	grcTextureFactory::CreateParams params;
	params.UseFloat = true;
	params.Multisample = 0;
	params.HasParent = true;
	params.Parent = NULL;
	params.IsResolvable = false;
	params.IsRenderable = true;
	params.UseHierZ = true;
	params.Format = grctfNone;

#if ENTITYSELECT_NO_OF_PLANES == 1
	//Create EDRAM depth target
	if(createFullsizeDepth)
		ES_Global::sEntitySelectDepth.Create("Entity Select Depth Target", grcrtDepthBuffer, ES_Global::sBackBufferWidth, ES_Global::sBackBufferHeight, sBPP, &params);
	else
		ES_Global::sEntitySelectDepth = fullsizeDepthTarget;
#else // ENTITYSELECT_NO_OF_PLANES == 1
	ES_Global::sEntitySelectDepth.Create("Entity Select Depth Target", grcrtDepthBuffer, ES_Global::sBackBufferWidth, ES_Global::sBackBufferHeight, sBPP, &params);
#endif // ENTITYSELECT_NO_OF_PLANES == 1

	params.HasParent = true;
	params.Parent = ES_Global::sEntitySelectDepth.GetRT(0);
	params.IsResolvable = false;
	params.IsRenderable = true;
	params.Format = grctfA8R8G8B8;

#if ENTITYSELECT_NO_OF_PLANES == 1
	//Create EDRAM RGB target
	if(createFullsizeRt)
		ES_Global::sEntitySelectRT.Create("Entity Select EDRAM target", grcrtPermanent, ES_Global::sBackBufferWidth, ES_Global::sBackBufferHeight, sBPP, &params);
	else
		ES_Global::sEntitySelectRT = fullsizeRGBTarget;
#else // ENTITYSELECT_NO_OF_PLANES == 1
	ES_Global::sEntitySelectRT.Create("Entity Select target", grcrtPermanent, ES_Global::sBackBufferWidth, ES_Global::sBackBufferHeight, sBPP, &params);
#endif // ENTITYSELECT_NO_OF_PLANES == 1

	params.HasParent = false;
	params.Parent = NULL;
	params.IsResolvable = true;
	params.IsRenderable = false;
	PS3_ONLY(params.InLocalMemory = false);	//We need to read this texture from the CPU, so don't create it in VRAM.

#if RSG_PC || RSG_ORBIS || RSG_DURANGO
	params.Lockable = true;
#endif
#if RSG_ORBIS
	params.ForceNoTiling = true;
#endif // RSG_ORBIS

	//Create resolve target.
	ES_Global::sEntitySelectResolve[0].Create("Entity Select Resolve Target 0", grcrtPermanent, ES_Global::sRtWidth, ES_Global::sRtHeight, sBPP, &params);
	ES_Global::sEntitySelectResolve[1].Create("Entity Select Resolve Target 1", grcrtPermanent, ES_Global::sRtWidth, ES_Global::sRtHeight, sBPP, &params);

	//On PS3, initialize our resolve target so it has a valid value the 1st frame it's enabled.
	PS3_ONLY(ES_Global::InitializeResolveTarget());
}

void CEntitySelect::ShutDown()
{
	ES_Global::sEntitySelectResolve[0].Release();
	ES_Global::sEntitySelectResolve[1].Release();
	ES_Global::sEntitySelectRT.Release();
	ES_Global::sEntitySelectDepth.Release();
	
	ES_Global::sESTechniqueGroupId = -1;
	ES_Global::sEntitySelectVarID = grcegvNONE;
	
	ES_Global::rgbWriteBS = grcStateBlock::BS_Invalid;
}

void CEntitySelect::LockTargets()
{
	Assert(ES_Global::sEntitySelectRT.IsValid() && ES_Global::sEntitySelectDepth.IsValid());


	const grcRenderTarget* rendertargets[grcmrtColorCount] = { NULL };

	for(int i=0; i<ENTITYSELECT_NO_OF_PLANES; i++)
		rendertargets[i] = ES_Global::sEntitySelectRT.GetRT(i);

	grcTextureFactory::GetInstance().LockMRT(rendertargets, ES_Global::sEntitySelectDepth.GetRT(0));
}

void CEntitySelect::UnlockTargets()
{
	Assert(ES_Global::sEntitySelectRT.IsValid());

#	if __XENON
	ES_Global::ResolveTargets();
#	endif //__XENON

	grcTextureFactory::GetInstance().UnlockMRT();

#	if !__XENON
	ES_Global::CopyESRT();
#	endif
}

void CEntitySelect::BeginExternalPass()
{
	ES_Global::sIsEntitySelectExternalPassCurrent = true;
}

void CEntitySelect::EndExternalPass()
{
	ES_Global::sIsEntitySelectExternalPassCurrent = false;
}

void CEntitySelect::PrepareGeometryPass()
{
	if(Verifyf(ES_Global::IsInitialized(), "WARNING! CEntitySelect::PrepareGeometryPass() called but it appears the system has not yet been initialized! Call ignored!"))
	{
		ES_Global::sIsEntitySelectPassCurrent = true;

		//Setup the render targets
		LockTargets();

		//Setup the render states!
		SetRenderStates();

		//Force the Entity Select technique!
		ES_Global::SetESForcedTechnique();
	}
}

void CEntitySelect::FinishGeometryPass()
{
	if(Verifyf(ES_Global::IsInitialized(), "WARNING! CEntitySelect::FinishGeometryPass() called but it appears the system has not yet been initialized! Call ignored!"))
	{
		UnlockTargets();

		//Restore previously forced technique
		ES_Global::ClearESForcedTechnique();

		//Lastly, read back the currently selected entity Id from the texture and store the safe RegdRef ptr.
		ES_Global::sSampledEntityId = ES_Global::SampleCurrentEntityId();
	}

	ES_Global::sIsEntitySelectPassCurrent = false;
}

void CEntitySelect::ClearResolveTargets()
{
	ES_Global::ClearResolveTargets();
}

void CEntitySelect::SetRenderStates()
{
	grcStateBlock::SetStates(grcStateBlock::RS_Default, CShaderLib::DSS_LessEqual_Invert, grcStateBlock::BS_Default);

	//Setup scissor so we only render into desired region.
	if(ES_Global::sSetScissor)
	{
		int bufferId = gRenderThreadInterface.GetRenderBuffer();
		GRCDEVICE.SetScissor(ES_Global::sWindowRects[bufferId].X1, ES_Global::sWindowRects[bufferId].Y1, ES_Global::sRtWidth, ES_Global::sRtHeight);
	}
}

void CEntitySelect::GetSelectScreenQuad(Vector2& pos, Vector2& size)
{
	int bufferId = gRenderThreadInterface.GetCurrentBuffer();

	pos = (ES_Global::sVisualizeCenterOnMouse ? Vector2((float)ES_Global::sWindowRects[bufferId].X1, (float)ES_Global::sWindowRects[bufferId].Y1) : Vector2((float)ES_Global::sVisualizePosX, (float)ES_Global::sVisualizePosY));
	size = (ES_Global::sVisualizeOverrideSize ? Vector2((float)ES_Global::sVisualizeWidth, (float)ES_Global::sVisualizeHeight) : Vector2((float)ES_Global::sRtWidth, (float)ES_Global::sRtHeight));

	//Make sure that enlarged visualizations at the mouse position are centered appropriately.
	if(ES_Global::sVisualizeCenterOnMouse && ES_Global::sVisualizeOverrideSize)
	{
		int halfLargerX = Max((ES_Global::sVisualizeWidth - static_cast<int>(ES_Global::sRtWidth)) / 2, 0);
		int halfLargerY = Max((ES_Global::sVisualizeWidth - static_cast<int>(ES_Global::sRtWidth)) / 2, 0);

		pos.Set((float)Max<int>(ES_Global::sWindowRects[bufferId].X1 - halfLargerX, 0), (float)Max<int>(ES_Global::sWindowRects[bufferId].Y1 - halfLargerY, 0));
	}
}

void CEntitySelect::DrawDebug()
{
	if(ES_Global::IsInitialized() && ES_Global::sVisualizeEntitySelectRT)
	{
		int bufferId = gRenderThreadInterface.GetRenderBuffer();
		CEntitySelectDebugDraw renderer;
		// Can only draw 1 plane atm.
		renderer.PreDebugRender(ES_Global::sEntitySelectResolve[bufferId].GetRT(ES_Global::sVisualizeEntitySelectPlane), true, ES_Global::rgbWriteBS, grcStateBlock::DSS_IgnoreDepth);
		if(ES_Global::sVisualizeFullscreen)
			renderer.DrawFullscreen();
		else
		{
			Vector2 quadPos, quadSize;
			CEntitySelect::GetSelectScreenQuad( quadPos, quadSize );
			renderer.DrawScreenQuad(quadPos, quadSize);
		}
		renderer.PostDebugRender();
	}
}

entitySelectID CEntitySelect::GetSelectedEntityId()
{
	return ES_Global::sSampledEntityId;
}

fwEntity* CEntitySelect::GetSelectedEntity()
{
	const entitySelectID id = GetSelectedEntityId();
	return CEntityIDHelper::GetEntityFromID(id);
}

int CEntitySelect::GetSelectedEntityShaderIndex()
{
	const entitySelectID id = GetSelectedEntityId();
	return CEntityIDHelper::GetEntityShaderIndexFromID(id);
}

const grmShader* CEntitySelect::GetSelectedEntityShader()
{
	const entitySelectID id = GetSelectedEntityId();

	if (CEntityIDHelper::IsEntityIdValid(id))
	{
		const CEntity* pEntity = (const CEntity*)CEntityIDHelper::GetEntityFromID(id);
		const int shaderIndex = CEntityIDHelper::GetEntityShaderIndexFromID(id);

		if (pEntity && shaderIndex >= 0)
		{
			if (pEntity->GetIsTypePed() && sEntitySelectEnablePedShaderHack) // peds are handled differently, since we can't get the shader from the drawable directly
			{
				return NULL;
			}
			else
			{
				const u32 modelIndex = pEntity->GetModelIndex();

				if (CModelInfo::IsValidModelInfo(modelIndex))
				{
					const CBaseModelInfo* modelInfo = CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(modelIndex)));

					if (modelInfo)
					{
						const Drawable* drawable = modelInfo->GetDrawable();

						if (drawable)
						{
							const grmShaderGroup& group = drawable->GetShaderGroup();

							if (shaderIndex < group.GetCount())
							{
								return group.GetShaderPtr(shaderIndex);
							}
						}
					}
				}
			}
		}
	}

	return NULL;
}

const char* CEntitySelect::GetSelectedEntityShaderName()
{
	const entitySelectID id = GetSelectedEntityId();

	if (CEntityIDHelper::IsEntityIdValid(id))
	{
		const grmShader* pShader = GetSelectedEntityShader();

		if (pShader)
		{
			return pShader->GetName();
		}
		else
		{
			const CEntity* pEntity = (const CEntity*)CEntityIDHelper::GetEntityFromID(id);

			if (pEntity && pEntity->GetIsTypePed() && sEntitySelectEnablePedShaderHack) // peds are handled differently, since we can't get the shader from the drawable directly
			{
				const int shaderIndex = CEntityIDHelper::GetEntityShaderIndexFromID(id);

				if (shaderIndex < NELEM(g_PedShaderNames))
				{
					return g_PedShaderNames[shaderIndex];
				}
			}
		}
	}

	return "";
}

static entitySelectID s_currentlyRenderingEntityId = CEntityIDHelper::GetInvalidEntityId();

void CEntitySelect::BindEntityIDShaderParam(entitySelectID id)
{
	if((ES_Global::sIsEntitySelectPassCurrent || ES_Global::sIsEntitySelectExternalPassCurrent) && ES_Global::IsInitialized() && IsEntitySelectPassEnabled()) //Avoid setting the entity ID when not necessary.
	{
		ES_Global::SetESIdShaderVars(ES_Global::ComputeESIdParamVal(id), CEntityIDHelper::IsEntityIdValid(id));
		s_currentlyRenderingEntityId = id;
	}
}

bool CEntitySelect::BindEntityIDShaderParamFunc(const grmShaderGroup& group, int shaderIndex)
{
	if (shaderIndex < 0 || shaderIndex >= BIT(CEntityIDHelper::ES_SHADER_INDEX_BITS)) // if shader index is too high, don't encode it
	{
		shaderIndex = CEntityIDHelper::ES_SHADER_INDEX_INVALID;
	}
	else
	{
		const grmShader& shader = group.GetShader(shaderIndex);
		const char* shaderName = shader.GetName();

		if (strstr(shaderName, "ped_") != NULL && sEntitySelectEnablePedShaderHack) // TODO -- determine if this is a ped in a more robust way
		{
			bool bFound = false;

			for (int i = 0; i < NELEM(g_PedShaderNames); i++)
			{
				if (strcmp(shaderName, g_PedShaderNames[i]) == 0)
				{
					shaderIndex = i;
					bFound = true;
					break;
				}
			}

			if (!bFound)
			{
				Assertf(0, "ped shader name '%s' not in g_PedShaderNames, please update list", shaderName);
				shaderIndex = CEntityIDHelper::ES_SHADER_INDEX_INVALID;
			}
		}
	}

	BindEntityIDShaderParam((s_currentlyRenderingEntityId & ~CEntityIDHelper::ES_SHADER_INDEX_MASK) | ((shaderIndex << CEntityIDHelper::ES_SHADER_INDEX_SHIFT) & CEntityIDHelper::ES_SHADER_INDEX_MASK));
	return true;
}

grcEffectGlobalVar CEntitySelect::GetEntityIDShaderVarID()
{
	return ES_Global::sEntitySelectVarID;
}

void CEntitySelect::AddWidgets(bkGroup &group)
{
	group.AddToggle("Enable Entity Select Render Pass", &sEntitySelectRenderPassEnabled);
	group.AddToggle("Enable Per-Shader Entity Select", &sEntitySelectUsingShaderIndexExtension);
	group.AddToggle("Enable Ped Shader Hack", &sEntitySelectEnablePedShaderHack);
	group.AddToggle("Visualize Entity Select Target", &ES_Global::sVisualizeEntitySelectRT);
#if ENTITYSELECT_NO_OF_PLANES > 1
	group.AddSlider("Plane to Visualize", &ES_Global::sVisualizeEntitySelectPlane, 0, ENTITYSELECT_NO_OF_PLANES - 1, 1);
#endif
	group.AddToggle("Scramble Bits (For better visualization)", &ES_Global::sScrambleBits);
	group.AddToggle("Set Scissor", &ES_Global::sSetScissor);
	group.AddText("Entity ID", reinterpret_cast<int *>(&ES_Global::sSampledEntityId), true);
	group.AddColor("Entity ID Colour", reinterpret_cast<Color32 *>(&ES_Global::sSampledEntityId));
#if ENTITYSELECT_NO_OF_PLANES > 1
	u32 *bits = reinterpret_cast<u32 *>(&ES_Global::sSampledEntityId);
	group.AddColor("Entity ID Colour - Higher Bits", reinterpret_cast<Color32 *>(bits+1));
#endif
	CompileTimeAssert(sizeof(Color32) == sizeof(u32));
}

#endif //ENTITYSELECT_ENABLED_BUILD
