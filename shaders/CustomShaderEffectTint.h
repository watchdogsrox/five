//
// CustomShaderTint - class for managing object palette tinting;
//
//	14/02/2010	-	Andrzej:	- intial;
//
//
//
//
#ifndef __CUSTOMSHADEREFFECT_TINT__
#define __CUSTOMSHADEREFFECT_TINT__

// rage includes:
#include "paging\ref.h"

// game includes:
#include "renderer\GtaDrawable.h"			// gtaDrawable
#include "physics\gtaArchetype.h"			// gtaFragType
#include "shaders\CustomShaderEffectBase.h"

#define	CSE_TINT_EDITABLEVALUES		(__BANK)

#define TINT_PALETTE_SET_IN_EDGE	(__PS3 && SPU_GCM_FIFO)	// ps3: tint palette is decompressed by EDGE

class CCustomShaderEffectTint;

//
//
//
//
class CCustomShaderEffectTintType: public CCustomShaderEffectBaseType
{
public:
	CCustomShaderEffectTintType() : CCustomShaderEffectBaseType(CSE_TINT) {}
	static CCustomShaderEffectTintType*		Create(rmcDrawable *pDrawable, CBaseModelInfo *pMI);

	virtual bool							Initialise(rmcDrawable *pDrawable);
	virtual CCustomShaderEffectBase*		CreateInstance(CEntity *pEntity);														// instance create in CEntity::CreateDrawable()
	virtual bool							RestoreModelInfoDrawable(rmcDrawable *pDrawable);

	void									SetIsHighDetail(bool flag)					{ m_bIsHighDetail = flag;	}
	bool									GetIsHighDetail() const						{ return m_bIsHighDetail;	}
	u8										GetShaderCount() const						{ return m_shaderCount;		}

	void									SetForceVehicleStencil(bool flag)			{ m_bForceVehicle = flag; }
	bool									GetForceVehicleStencil() const				{ return m_bForceVehicle; }

protected:
	virtual ~CCustomShaderEffectTintType();

public:
	enum { RscTintInfoOffset = 8*sizeof(u8) };	// offset to tintShaderInfo from beginning of actual resource

	// when doing any changes here, then update \framework\tools\src\cli\ragebuilder\gta_res.h
	struct structTintShaderInfo
	{
		u8				DECLARE_BITFIELD_2(	m_DefaultPalette,	7,		// currently selected palette
											m_bIsTree,			1);		// "IsTree" flag (ps3: special unpack path in EDGE)
		u8				m_PaletteTexHeight;			// ps3/360: # of available palettes; rest of the world: texture height
#if TINT_PALETTE_SET_IN_EDGE
		u8				m_PaletteOffset;			// offset to first palette in big palette buffer
#else
		u8				m_varPaletteTintSelector;	// 360: 0,1,2,...
#endif
		u8				m_ShaderIdx;				// ps3: shader idx in shaderGroup and tintDesc[]
	};
	CompileTimeAssert(sizeof(structTintShaderInfo)==4);

private:
	atArray<structTintShaderInfo>	m_privTintInfos;	// at *most* one per shader; DO NOT USE DIRECTLY (invalid when m_bResourced=true), use GetTintInfos() instead;
	pgRef<gtaDrawable>				m_pDrawable;		// valid only when m_bResourcedFrag=false
	pgRef<gtaFragType>				m_pFrag;			// valid only when m_bResourcedFrag=true

	u8	m_shaderCount;									// ... total number of shaders, >= m_tintInfos.GetCount, and also the number of tint descriptors we need
	u8	m_bIsHighDetail		: 1;
	u8	m_bResourced		: 1;						// if true then tintInfos (and m_tintPalettes on ps3) have been resourced from disk
	u8	m_bResourcedFrag	: 1;						// if true, then it's been resourced from gtaFrag otherwise gtaDrawable
	u8	m_bForceVehicle		: 1;						// Force to use vehicle stencil ref
	ATTR_UNUSED u8	m_flagPad			: 7;
	ATTR_UNUSED u8	m_pad[2] ;

	enum { TintPaletteSize=256 };	// 256 colors in tint palette
#if TINT_PALETTE_SET_IN_EDGE
public:
	struct Palette 
	{ 
		u32 entries[TintPaletteSize]; 
	};

private:
	atArray <Palette> m_privTintPalettes;				// DO NOT USE DIRECTLY, use GetTintPalettes() instead
	static bool UnswizzleTextureData(void *dstPixels, const void *srcPixels, u32 bpp, u32 width, u32 height);
#endif


public:
	structTintShaderInfo*					GetTintInfos(atUserArray<structTintShaderInfo>* userTintInfos);
#if TINT_PALETTE_SET_IN_EDGE
	Palette*								GetTintPalettes(atUserArray<Palette>* userTintPalettes);
#endif
};

//
//
//
//
class CCustomShaderEffectTint : public CCustomShaderEffectBase
{
	friend class CCustomShaderEffectTintType;

public:
	~CCustomShaderEffectTint();

public:
	virtual void						Update(fwEntity *pEntity);																// instance update in CEntity::PreRender()

	virtual void						SetShaderVariables(rmcDrawable* pDrawable);																	// RT: instance setting variables in BeforeDraw()
	virtual void						AddToDrawList(u32 modelIndex, bool bExecute);															// DLC: called from BeforeAddToDrawList()
	virtual void						AddToDrawListAfterDraw();																// DLC: called from AfterAddToDrawList()
	virtual u32							AddDataForPrototype(void * address);

	void								SelectTintPalette(u8 selpal, fwEntity* pEntity);
	u32									GetMaxTintPalette();
	u32									GetNumTintPalettes();

	const CCustomShaderEffectTintType*	GetCseType() const								{ return m_pType; }

	//Instancing support!
	virtual bool HasPerBatchShaderVars() const		{ return true; }

	static void							SelectTintPalette(u8 selpal, u8 *result, CCustomShaderEffectTintType *type, fwEntity* pEntity);
	static void							SetShaderVariables(u8 *selPal, CCustomShaderEffectTintType *type, rmcDrawable* pDrawable);																	// RT: instance setting variables in BeforeDraw()

private:
	CCustomShaderEffectTint(u32 size);

#if TINT_PALETTE_SET_IN_EDGE
	static bool UnswizzleTextureData(void *dstPixels, const void *srcPixels, u32 bpp, u32 width, u32 height);
#endif


#if CSE_TINT_EDITABLEVALUES
public:
	static bool							InitWidgets(bkBank& bank);

	static bool							ms_bEVEnableForceTintPalette;
	static u32							ms_nEVForcedTintPalette;
#endif //CSE_TINT_EDITABLEVALUES...

private:
	CCustomShaderEffectTintType			*m_pType;
#if TINT_PALETTE_SET_IN_EDGE
	u32									m_currentPaletteBytes;
	u32									m_pad0[2];
#endif

	u8									m_currentPalette[0];

#if TINT_PALETTE_SET_IN_EDGE
	u32*	GetTintDesc(u32 i)
	{
		// One of these per shader, not per tint info.
		TrapGE(i, (u32)m_pType->GetShaderCount());
		return (u32*)(m_currentPalette + m_currentPaletteBytes) + i;
	}

	u32*	GetTintPalette(u32 i)
	{
		atUserArray<CCustomShaderEffectTintType::Palette> tintPalettes;
		m_pType->GetTintPalettes(&tintPalettes);
		return &tintPalettes[i].entries[0];
	}
#endif
};


#endif //__CUSTOMSHADEREFFECT_TINT__....
