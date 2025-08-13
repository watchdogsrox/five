//
// name:		sprite2d.h
// description:	Class encapsulating a sprite to be rendered to the screen
//
//
//
//
#ifndef INC_SPRITE2D_H
#define INC_SPRITE2D_H

#include "paging/ref.h"
#include "grcore/texture.h"
#include "fwmaths/rect.h"
#include "camera/viewports/Viewport.h"
#if __D3D11
#include "renderer/Deferred/DeferredConfig.h"
#endif
#include "streaming/streamingdefs.h"

#define USE_MULTIHEAD_FADE			(__WIN32PC && SUPPORT_MULTI_MONITOR)
#define USE_HQ_ANTIALIASING			((__BANK && (__PS3 || __XENON )) || __D3D11 || RSG_ORBIS) // Also defined in im.fx

#if USE_MULTIHEAD_FADE
	#define MULTIHEAD_ONLY(x) x
#else
	#define MULTIHEAD_ONLY(x)
#endif

//
//
//
//
namespace rage
{
	class Vector2;
	class Vector4;
	class Color32;
	class grcTexture;
	class grcViewport;
	class grmShader;
}

// ================================================================================================
// ================================================================================================
// ================================================================================================

// ================================================================================================
// ================================================================================================
// ================================================================================================

//
//
//
//
class CSprite2d
{
public:
	CSprite2d()													{ m_pTexture = NULL; }
	CSprite2d(const char* pTextureName) : m_pTexture(NULL)		{ SetTexture(pTextureName); }
	CSprite2d(grcTexture *pTexture) : m_pTexture(NULL)	{ SetTexture(pTexture); };
	~CSprite2d()												{ Delete(); }

	static void		Init(unsigned initMode);
	static void		Shutdown(unsigned shutdownMode);

	void			Delete();
	void			SetTexture(const char *pTextureName);
	void			SetTexture(grcTexture *pTexture);
	const grcTexture* GetTexture() const { return(m_pTexture); }
	bool			HasTexture() const { return m_pTexture != NULL; }
	void			SetRenderState();
	static void		SetRenderState(const grcTexture* pTex);
	static void		ClearRenderState();
	static grmShader* GetShader() { return m_imShader; }
	// Custom shader passes.
	
	enum CustomShaderType {
		CS_CORONA_DEFAULT,
		CS_CORONA_SIMPLE_REFLECTION, // mirror, water
		CS_CORONA_PARABOLOID_REFLECTION,
#if GS_INSTANCED_CUBEMAP
		CS_CORONA_CUBEINST_REFLECTION,
#endif
		CS_CORONA_LIGHTNING,
		CS_COLORCORRECTED,
		CS_COLORCORRECTED_GRADIENT,
		CS_SEETHROUGH,
		CS_MIPMAP_BLUR_0,
		CS_MIPMAP_BLUR_BLEND,
		CS_REFLECTION_SKY_PORTALS,
#if __PPU
		CS_RESOLVE_MSAA2X,
		CS_RESOLVE_MSAA4X,
		CS_RESOLVE_SDDOWNSAMPLEFILTER,
		CS_RESOLVE_DEPTHTOVS,
		CS_RESOLVE_ALPHASTENCIL,
		CS_IM_LIT_PACKED,
		CS_IM_UNLIT_PACKED,
		CS_ZCULL_RELOAD,
		CS_ZCULL_RELOAD_GT,
		CS_ZCULL_NEAR_RELOAD,
		CS_SCULL_RELOAD,
#endif // __PPU		
		CS_GBUFFER_SPLAT,
		CS_ALPHA_MASK_TO_STENCIL_MASK,
#if __XENON
		CS_ALPHA_MASK_TO_STENCIL_MASK_STENCIL_AS_COLOR,
		CS_RESTORE_HIZ,
		CS_RSTORE_GBUFFER_DEPTH,
#endif // __XENON
		CS_COPY_GBUFFER,
		CS_RESOLVE_DEPTH,
		CS_BLIT,
		CS_BLIT_NOFILTERING,
		CS_BLIT_DEPTH,
		CS_BLIT_STENCIL,
		CS_BLIT_SHOW_ALPHA,
		CS_BLIT_GAMMA,
		CS_BLIT_POW,
		CS_BLIT_CALIBRATION,
		CS_BLIT_CUBE_MAP,
		CS_BLIT_PARABOLOID,
		CS_BLIT_AA,
		CS_BLIT_AAAlpha,
		CS_BLIT_COLOUR, // solid colour, no texture
		CS_BLIT_TO_ALPHA,
		CS_BLIT_TO_ALPHA_BLUR,
		CS_BLIT_DEPTH_TO_ALPHA,  // Blit colour and depth into alpha
		CS_BLIT_TEX_ARRAY,
#if DEVICE_MSAA
		CS_BLIT_MSAA_DEPTH,
		CS_BLIT_MSAA_STENCIL,
		CS_BLIT_MSAA,
		CS_BLIT_FMASK,
#endif
		CS_DISTANT_LIGHTS,
		CS_FOWCOUNT,
		CS_FOWAVERAGE,
		CS_FOWFILL,
		CS_GAMEGLOWS,
		CS_FSGAMEGLOWS,
		CS_FSGAMEGLOWSMARKER,
		CS_COPYDEPTH,
		CS_TONEMAP,
		CS_LUMINANCE_TO_ALPHA,
#if USE_HQ_ANTIALIASING
		CS_HIGHQUALITY_AA,
#endif
		CS_BLIT_XZ_SWIZZLE,
		CS_MIPMAP,
		CS_RESOLVE_DEPTH_MS_SAMPLE0,
		CS_RESOLVE_DEPTH_MS_MIN,
		CS_RESOLVE_DEPTH_MS_MAX,
		CS_RESOLVE_DEPTH_DOMINANT,

		CS_SIGNED_AS_UNSIGNED,

		CS_COUNT
	};

	void			SetRefMipBlurParams(const Vector4& params, const grcTexture *pTex = NULL);
	void			SetRefMipBlurMap(grcTexture *tex);
	void			SetLightningConstants(const Vector2& params);
	void			SetTexelSize(const Vector2 &size);
	void			SetTexelSize(const Vector4 &size);

	void			SetColorFilterParams(const Vector4 &colorFilter0, const Vector4 &colorFilter1);

	void			SetDepthTexture(grcTexture *tex);

	void			SetNoiseTexture(grcTexture *tex);
		
	static void		SetGeneralParams(const Vector4 &params0, const Vector4 &params1);

	void			BeginCustomList(CustomShaderType type, const grcTexture *pTex, const int pass = -1);
	void			EndCustomList();

#if DEVICE_MSAA
	void			BeginCustomListMS(CustomShaderType type, const grcTexture *pTex, const int pass = -1);
	void			EndCustomListMS();
#endif

private:
	static void		SetBlitMatrix(grcViewport *vp=NULL, fwRect *rect=NULL, bool bForce=FALSE);

public:
	static void		OverrideBlitMatrix(grcViewport& viewport);
	static void		OverrideBlitMatrix(float width, float height);
	static void		OverrideBlitMatrix(float left, float right, float top, float bottom);
	static void		ResetBlitMatrix();

	// use this with care, at it 	slows down sprite2d rendering
	static void		SetAutoRestoreViewport(bool bRestore);
	static void		StoreViewport(grcViewport *vp);
	static void		RestoreLastViewport(bool bForce=TRUE);

	static void		ResetDefaultScreenVP();

	// Immediate mode utils
	static void		Draw(	const Vector2 &pt1, const Vector2 &pt2, const Vector2 &pt3, const Vector2 &pt4, 
							const Color32 &rgba);
	static void		Draw(	const Vector2 &pt1, const Vector2 &pt2, const Vector2 &pt3, const Vector2 &pt4, 
							const Vector2 &uv1, const Vector2 &uv2, const Vector2 &uv3, const Vector2 &uv4, 
							const Color32 &rgba);

	static void		DrawSprite3D(float x, float y, float z, float width, float height, const Color32& color, const grcTexture* pTex, const Vector2& uv0, const Vector2& uv1);

	// textured rect
	static void		DrawRect(	const Vector2 &pt1, const Vector2 &pt2, const Vector2 &pt3, const Vector2 &pt4, float zVal,
								const Vector2 &uv1, const Vector2 &uv2, const Vector2 &uv3, const Vector2 &uv4, 
								const Color32 &color);

	static void		DrawUIWorldIcon(	const Vector3 &pt1, const Vector3 &pt2, const Vector3 &pt3, const Vector3 &pt4, float zVal,
		const Vector2 &uv1, const Vector2 &uv2, const Vector2 &uv3, const Vector2 &uv4, 
		const Color32 &color);

	// Untextured rect
	static void			DrawRect(float x1, float y1, float x2, float y2, float zVal, const Color32& color);
	inline static void	DrawRect(const fwRect &r, const Color32 &rgba );
	inline static void	DrawRectSlow(const fwRect &r, const Color32 &rgba );

	// Multi-monitor GUI drawing
	static void	DrawRectGUI(const fwRect &r, const Color32 &rgba);
	inline static void DrawRectSwitch(bool bGUI, const fwRect &r, const Color32 &rgba);


	static void DrawSwitch(	bool bGUI, bool bRect, float zVal,
							const Vector2 &pt1, const Vector2 &pt2, const Vector2 &pt3, const Vector2 &pt4, 
							const Vector2 &uv1, const Vector2 &uv2, const Vector2 &uv3, const Vector2 &uv4, 
							const Color32 &rgba);

	inline static void DrawSwitch(	bool bGUI, bool bRect, float zVal,
							const Vector2 &pt1, const Vector2 &pt2, const Vector2 &pt3, const Vector2 &pt4, 
							const Color32 &rgba);
	
	// more open-ended shape drawing func (same as DrawRect but you specify a function pointer/lambda to draw the vertices)
	template<class T> static void DrawCustomShape( T DrawFunc )
	{
		GRC_ALLOC_SCOPE_AUTO_PUSH_POP();

		PreDrawShape();

		DrawFunc();

		PostDrawShape();
	}

#if SUPPORT_MULTI_MONITOR
	static bool MoveToScreenGUI(Vector2 *const points, const int number);
	static void MovePosVectorToScreenGUI(Vector2 &pos, Vector2 &vec);

	template<int N>
	static bool MoveToScreenGUI(Vector2 (&points)[N])
	{
		return MoveToScreenGUI(points,N);
	}
#endif	//SUPPORT_MULTI_MONITOR
	
#if USE_MULTIHEAD_FADE
	static u32	GetFadeCurrentTime();
	static u32 SetMultiFadeParams(
		const fwRect &safeArea, bool bIn, u32 uDurationSec,
		float fAcceleration, float fWaveSize, float fAlphaOffset,
		bool bInstantShudders = false, u32 uExtraTime = 0);
	static void ResetMultiFadeArea();
	static void	DrawMultiFade(bool bUseTIme);
#endif	//USE_MULTIHEAD_FADE

private:
	static void PreDrawShape();
	static void PostDrawShape();

	pgRef<grcTexture> m_pTexture;
	static grmShader *m_imShader;
	static grmShader *m_imShaderMS;

#if PGHANDLE_REF_COUNT
	CustomShaderType m_type;
#endif
};

//
// name:		CSprite2d::DrawRect
// description:	Draw a 2d rectangle with solid colour.
inline void CSprite2d::DrawRect(const fwRect &r, const Color32 &rgba)
{
	CSprite2d::DrawRect(r.left, r.top, r.right, r.bottom, 0.0f, rgba);
}

// converts vp mapping (0,0)->(W,H) into (0,0)->(1,1)
inline void CSprite2d::DrawRectSlow(const fwRect &rSWH, const Color32 &rgba)
{
	fwRect r(rSWH.left/float(SCREEN_WIDTH), rSWH.bottom/float(SCREEN_HEIGHT), rSWH.right/float(SCREEN_WIDTH), rSWH.top/float(SCREEN_HEIGHT));
	CSprite2d::DrawRect(r.left, r.top, r.right, r.bottom, 0.0f, rgba);
}

//
// name:		CSprite2d::DrawRectSwitch
// description:	draws a rectangle either for GUI or in general depending on the first boolean argument
inline void CSprite2d::DrawRectSwitch(bool bGUI, const fwRect &r, const Color32 &rgba)
{
	if (bGUI)
		DrawRectGUI(r,rgba);
	else
		DrawRect(r,rgba);
}

inline void CSprite2d::DrawSwitch(	bool bGUI, bool bRect, float zVal,
						const Vector2 &pt1, const Vector2 &pt2, const Vector2 &pt3, const Vector2 &pt4, 
						const Color32 &rgba)
{
	static const Vector2 uv[] = {
		Vector2(0.f,1.f), Vector2(0.f,0.f),
		Vector2(1.f,1.f), Vector2(1.f,0.f)
	};
	DrawSwitch( bGUI, bRect, zVal, pt1,pt2,pt3,pt4, uv[0],uv[1],uv[2],uv[3], rgba );
}


////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
class CUIWorldIconQuad
{
public:
	CUIWorldIconQuad();
	~CUIWorldIconQuad();

	void Clear();
	void Update();
	void AddToDrawList();

	void SetPosition(Vector3& rPosition) {m_worldPosition = rPosition;}
	void SetSize(Vector2& rSize) {m_size = rSize;}
	void SetSize(float width, float height) {m_size.x = width; m_size.y = height;}

	void SetColor(const Color32& rColor) {m_color = rColor;}
	void SetUseAlpha(bool useAlpha) {m_useAlpha = useAlpha;}

	void SetTexture(const char* pTextName, const char* pTextDictionary);

	const Vector3& GetPosition() const {return m_worldPosition;}

	float GetWidth() const;
	float GetHeight() const;

	bool HasTexture() const;

private:
	void LoadTexure();

	static void InitCommon();

	Vector3 m_worldPosition;
	Vector2 m_size;

	Color32 m_color;
	CSprite2d m_sprite;

	bool m_useAlpha;
	bool m_isTextureSet;

	strLocalIndex m_textureSlot;
	const char* m_pTextureName;
	const char* m_pTextureDictionary;

	static bool sm_initializedCommon;
	static grcBlendStateHandle sm_StandardSpriteBlendStateHandle;
	static float sm_screenRatio;
};


#endif //INC_SPRITE2D_H...


