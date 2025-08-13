//
// CustomShaderTint - class for managing object palette tinting;
//
//	14/02/2010	-	Andrzej:	- intial;
//
//
// note: 
// TintPaletteTex - texWidth and texHeight must be divisible by 4 (360/ps3 limitation) - see grcTextureXenon::grcTextureXenon() and grcTextureGCM::grcTextureGCM();
//
//
//
// Rage headers:
#include "grcore\textureGcm.h"
#if __XENON
	#include "system/xtl.h"
	#define DBG 0
	#include <xgraphics.h>
	#undef DBG
#endif
#include "grcore\texturexenon.h"
#include "grcore\grcorespu.h"
#include "diag\art_channel.h"

#include "fwdrawlist/drawlistmgr.h"

// Game headers:
#include "renderer\render_channel.h"	// renderWarningf()...
#include "shaders\ShaderLib.h"
#include "scene\Entity.h"
#include "streaming\defragmentation.h"
#include "renderer\GtaDrawable.h"
#include "shaders\CustomShaderEffectTint.h"

//#include "system/findsize.h"
//FindSize(CCustomShaderEffectTint);

#if __BANK
	PARAM(tintdisable,"Disable tint tech (may produce flashing visuals, etc.).");
#endif

// bright pink is end-of-palette marker:
#define END_OF_PALETTE_MARKER		(0xFFFF7FFF)	// A=255,R=255,G=127,B=255

inline Color32 RawColorToColor32(Color32 colorDC, grcTexture::eTextureSwizzle *remap);

//
//
//
//
#define VARNAME_TINT_PALETTE_TEX	("TintPaletteTex")
#define VARNAME_TINT_PAL_SELECTOR	("TintPaletteSelector")


#if __ASSERT
	extern char* CCustomShaderEffectBase_EntityName;	// defined in CustomShaderEffectBase
#endif


//
//
// master initialize in CBaseModelInfo::SetDrawable()
//
CCustomShaderEffectTintType* CCustomShaderEffectTintType::Create(rmcDrawable *pDrawable, CBaseModelInfo *pMI)
{
	Assert(pDrawable);
	if(!pDrawable)
		return(NULL);

	grmShaderGroup *pShaderGroup = &pDrawable->GetShaderGroup();
	const s32 shaderCount	= pShaderGroup->GetCount();
	Assert(shaderCount >= 1);

CCustomShaderEffectTintType* pType = NULL;

	atArray<u8> *pTintData = ((gtaDrawable*)pDrawable)->m_pTintData;	// if drawable, then rsc tint data comes from gtaDrawable
	gtaFragType *gtaFrag = (gtaFragType*)pMI->GetFragType();
	if(gtaFrag)
	{
		pTintData = gtaFrag->m_pTintData;	// if frag, then rsc tint data may come from gtaFragType
	}

	if(TINT_PALETTE_RESOURCES && pTintData && pTintData->GetCount() > 0)
	{	// setup tint straight from resources:
		u8 *tintPaletteData = pTintData->GetElements();
		Assert(tintPaletteData);
		Assert16(tintPaletteData);

		// first things first: check header:
		u8 *tintHdrData = NULL;
		#if TINT_PALETTE_SET_IN_EDGE
			const u32 tintPaletteSize	= pTintData->GetCount() &~0x3FF;
			Assert(tintPaletteSize>0);
			const u32 tintPalettesCount	= tintPaletteSize >> 10;
			Assert(tintPalettesCount>0);
			const u32 tintHeaderSize	= pTintData->GetCount() & 0x3FF;
			Assert(tintHeaderSize>0);

			tintHdrData = tintPaletteData + tintPaletteSize;
			Assert(tintHdrData[0] == 'T');
			Assert(tintHdrData[1] == 'N');
			Assert(tintHdrData[2] == 'T');
			Assert(tintHdrData[3] == 'C');
			Assert(tintHdrData[4] == 0);
			Assert(tintHdrData[5] == 0);
			Assert(tintHdrData[6] == 0);
		#elif __XENON
			#if __ASSERT
				const u32 tintHeaderSize	= pTintData->GetCount() & 0x3FF;
				Assert(tintHeaderSize>0);
			#endif
			tintHdrData = tintPaletteData;
			Assert(tintHdrData[0] == 'T');
			Assert(tintHdrData[1] == 'N');
			Assert(tintHdrData[2] == 'T');
			Assert(tintHdrData[3] == 'X');
			Assert(tintHdrData[4] == 0);
			Assert(tintHdrData[5] == 0);
			Assert(tintHdrData[6] == 0);
		#else
		(void)tintPaletteData;
		#endif
		const u32 numAllocTintShaderInfos = tintHdrData[7];
		Assert(numAllocTintShaderInfos>0);
		Assert(numAllocTintShaderInfos < 256);	// must fit into u8

		tintHdrData += RscTintInfoOffset;

		pType = rage_new CCustomShaderEffectTintType();

		Assert(shaderCount < 256);				// must fit into u8
		pType->m_shaderCount = (u8)shaderCount;

	#if TINT_PALETTE_SET_IN_EDGE
		atUserArray<Palette> *userTintPalettes = (atUserArray<Palette>*) &pType->m_privTintPalettes;
		userTintPalettes->Assume((Palette*)NULL, 0);
		*(userTintPalettes->GetCountPointer()) = tintPalettesCount;

		atUserArray<structTintShaderInfo> *userTintInfos = (atUserArray<structTintShaderInfo>*) &pType->m_privTintInfos;
		userTintInfos->Assume((structTintShaderInfo*)NULL, (u16)0);			// mark array as invalid
		*(userTintInfos->GetCountPointer()) = (u16)numAllocTintShaderInfos;	// ...but GetCount() should still returns valid info
	#elif __XENON
		atUserArray<structTintShaderInfo> *userTintInfos = (atUserArray<structTintShaderInfo>*) &pType->m_privTintInfos;
		userTintInfos->Assume((structTintShaderInfo*)NULL, (u16)0);			// mark array as invalid
		*(userTintInfos->GetCountPointer()) = (u16)numAllocTintShaderInfos;	// ...but GetCount() should still returns valid info
	#else
		(void)numAllocTintShaderInfos;
	#endif


		gtaFragType *gtaFrag = (gtaFragType*)pMI->GetFragType();
		if(gtaFrag)
		{
			Assert(pType->m_pFrag==NULL);
			pType->m_pFrag = gtaFrag;
			pType->m_bResourcedFrag = true;
		}
		else
		{
			Assert(pType->m_pDrawable==NULL);
			pType->m_pDrawable = (gtaDrawable*)pDrawable;
			pType->m_bResourcedFrag = false;
		}

		pType->m_bResourced = true;
	}
	else
	{	// setup tint palettes by hand:
		#if TINT_PALETTE_RESOURCES
			artAssertf(false, "CSETint: Object '%s' uses slow non-resourced tint data! This is an art bug and requires re-exporting with latest Tools.", CCustomShaderEffectBase_EntityName);
			// Errorf when we hit the non-resourced tint path. This can cause a hang on the 360 so I want it in the logs when these bugs come in.
            NOTFINAL_ONLY( extern char* CCustomShaderEffectBase_EntityName);
			NOTFINAL_ONLY( Errorf("ART BUG (CSETint): Object '%s' uses slow non-resourced tint data! This is an art bug and requires re-exporting with latest Tools.", CCustomShaderEffectBase_EntityName) );
		#endif

		// array of counted palettes for each shader:
		u32* shaderNumPalettes = Alloca(u32, shaderCount);
		sysMemSet(shaderNumPalettes, 0x00, shaderCount*sizeof(u32));
		
		u32 numAllocTintShaderInfos	= 0;
	#if TINT_PALETTE_SET_IN_EDGE
		u32 numAllocTintDescs		= 0;
		u32 numAllocTintPalettes	= 0;
	#endif
	#if TINT_PALETTE_SET_IN_EDGE || __XENON
		grcTexture::eTextureSwizzle texColRemap[4]; // r,g,b,a
	#endif

		for(s32 j=0; j<shaderCount; j++)
		{
			grmShader		*pShader = &pShaderGroup->GetShader(j);
			Assert(pShader);

			grcEffectVar	effectVarTintPalTex		= pShader->LookupVar(VARNAME_TINT_PALETTE_TEX, FALSE);
			grcEffectVar	effectVarTintPalSelector= pShader->LookupVar(VARNAME_TINT_PAL_SELECTOR, FALSE);

	#if TINT_PALETTE_SET_IN_EDGE
			numAllocTintDescs++;	// tintDesc for every shader in drawable, tinted or not
	#endif

			if(effectVarTintPalTex && effectVarTintPalSelector)
			{
	#if __ASSERT || TINT_PALETTE_SET_IN_EDGE || __XENON
				// find out how many palettes are stored in texture:
				const grcInstanceData& instanceData = pShader->GetInstanceData();
				const int index = (int)effectVarTintPalTex - 1;
				grcInstanceData::Entry *pEntry = &instanceData.Data()[index];
				grcTexture *pTintTexture = pEntry->Texture;
				Assertf(pTintTexture, "%s: TintPalTexture not found!", CCustomShaderEffectBase_EntityName);
				if (!pTintTexture)
					continue;
					
				const bool bIsWidthAppropriate	= (pTintTexture->GetWidth()==256);
				const bool bIsHeightAppropriate = (pTintTexture->GetHeight()>=4);
				const bool bIsBPPAppropriate	= (pTintTexture->GetBitsPerPixel()==32);

				// check texture dimensions and format (ARGB8888==32bpp):
				artAssertf(bIsWidthAppropriate,	"%s: TintPalTexture '%s' must have width=256.", CCustomShaderEffectBase_EntityName, pTintTexture->GetName());
				artAssertf(bIsHeightAppropriate,"%s: TintPalTexture '%s' must have height>=4.", CCustomShaderEffectBase_EntityName, pTintTexture->GetName());	// ps3 & 360 min. tex height is 4
				artAssertf(bIsBPPAppropriate,	"%s: TintPalTexture '%s' must be uncompressed 32bpp (ARGB8888).", CCustomShaderEffectBase_EntityName, pTintTexture->GetName());
				//Printf("\n tint palette: w=%d, h=%d, bpp=%d", pTintTexture->GetWidth(), pTintTexture->GetHeight(), pTintTexture->GetBitsPerPixel());
				if(!bIsWidthAppropriate || !bIsHeightAppropriate || !bIsBPPAppropriate)
					continue;
	#endif

				numAllocTintShaderInfos++;

	#if TINT_PALETTE_SET_IN_EDGE
				grcTextureGCM *pTintTextureGcm = static_cast<grcTextureGCM*>(pTintTexture);
				Assert(pTintTextureGcm);
				CellGcmTexture *cellTex = pTintTextureGcm->GetTexturePtr();
				Assert(cellTex);

				// is texture swizzled?
				const bool bIsSwizzled = !(cellTex->format&CELL_GCM_TEXTURE_LN);
				if(bIsSwizzled)
				{
					Assertf(0, "%s: TintPalTexture '%s' is not linear! Please set its TCP code to linear and re-export your resources.", CCustomShaderEffectBase_EntityName, pTintTexture->GetName());
				#if 1 //__BANK
					// ...then unswizzle:
					u32 *swizzledTexPtr = (u32*)pTintTextureGcm->GetBits();
					Assert(swizzledTexPtr);
					const u32 width = pTintTextureGcm->GetWidth();
					const u32 height= pTintTextureGcm->GetHeight();
					const u32 bpp	= 32;

					// allocate temp space on the stack:
					const u32 stackAllocSize = width*height*sizeof(u32);
					Assert(stackAllocSize <= 16*1024);	// don't want to alloc more than 16KB on stack
					u32 *unswizzledTexPtr = Alloca(u32, stackAllocSize/sizeof(u32));
					Assert(unswizzledTexPtr);
					if(!UnswizzleTextureData(unswizzledTexPtr, swizzledTexPtr, bpp, width, height))
					{
						Assertf(0, "Error unswizzling texture!");
					}

					// overwrite src tex data with unswizzled data:
					memcpy(swizzledTexPtr, unswizzledTexPtr, stackAllocSize);	// sysMemCpy() fails to copy from stack!

					// set linear flag:
					cellTex->format |= CELL_GCM_TEXTURE_LN;
				#else
					TrapZ(0);	// fatal: can't work with non-linear textures...
				#endif //__BANK...
				}// if(bIsSwizzled)...

				pTintTextureGcm->GetTextureSwizzle(texColRemap[0], texColRemap[1], texColRemap[2], texColRemap[3]);

				// find out # of real palettes (check for end-of-palette markers on col[0] place):
				u32 *tintPalettePtr = (u32*)pTintTextureGcm->GetBits();
				Assert(tintPalettePtr);
				u32 numPalettes = pTintTexture->GetHeight();
				
				const u32 height = pTintTexture->GetHeight();
				for(u32 p=0; p<height; p++)
				{
					Color32 *rawCol32 = (Color32*)(tintPalettePtr + p*CCustomShaderEffectTintType::TintPaletteSize);
					const Color32 firstColor = RawColorToColor32( rawCol32[0], texColRemap );
					
					if(firstColor.GetColor() == END_OF_PALETTE_MARKER)	// end-of-palette marker on first place?
					{
						numPalettes = p;
						break;
					}
				}
				Assertf(numPalettes > 0, "%s: No tint palettes detected in texture '%s'.", CCustomShaderEffectBase_EntityName, pTintTexture->GetName());
				Assertf(numPalettes <= pTintTexture->GetHeight(), "%s: Num tint palettes exceeds height of tint texture '%s'.", CCustomShaderEffectBase_EntityName, pTintTexture->GetName());

				numAllocTintPalettes += numPalettes;
				shaderNumPalettes[j] = numPalettes;	// store num palettes detected for this shader
	#endif //TINT_PALETTE_SET_IN_EDGE...

	#if __XENON
				grcTextureXenon *pTintTexture360 = (grcTextureXenon*)pTintTexture;
				Assert(pTintTexture360);
				D3DBaseTexture *d3dTex = pTintTexture360->GetTexturePtr();
				Assert(d3dTex);

				grcTextureLock lock;
				const bool bLocked = pTintTexture360->LockRect(0, 0, lock, grcsRead);
				Assertf(bLocked, "%s: Error locking tint palette %s.", CCustomShaderEffectBase_EntityName, pTintTexture->GetName());
				if(bLocked)
				{
					// only ARGB88888 swizzled or linear allowed:
					Assertf(lock.RawFormat==D3DFMT_A8R8G8B8 || lock.RawFormat==D3DFMT_LIN_A8R8G8B8, "%s: Incorrect xenon tint palette format detected in %s.",CCustomShaderEffectBase_EntityName, pTintTexture->GetName());

					const u32 width		= pTintTexture->GetWidth();
					const u32 height	= pTintTexture->GetHeight();

					u32* tintPalTmp = Alloca(u32, width*height);

					const bool bIsTiled = (lock.RawFormat==D3DFMT_A8R8G8B8);
					if(bIsTiled)
					{	
						D3DFORMAT format = static_cast<D3DFORMAT>(lock.RawFormat);
						Assert(XGIsTiledFormat(format));
						Assert(u32(lock.Pitch)==width*sizeof(u32));
						
						DWORD flags = 0;
						if (!XGIsPackedTexture(d3dTex))
						{
							flags |= XGTILE_NONPACKED;
						}
						if (XGIsBorderTexture(d3dTex))
						{
							flags |= XGTILE_BORDER;
						}
						
						// untile:
						XGUntileTextureLevel(width, height, /*Level*/0, /*GpuFormat*/XGGetGpuFormat(format), flags,
											/*pDestination*/tintPalTmp, /*RowPitch*/lock.Pitch, /*pPoint*/NULL, /*pSource*/lock.Base, /*pRect*/NULL);
					}
					else
					{
						sysMemCpy(tintPalTmp, lock.Base, width*height*sizeof(u32));
					}
					pTintTexture360->UnlockRect(lock);

					pTintTexture->GetTextureSwizzle(texColRemap[0], texColRemap[1], texColRemap[2], texColRemap[3]);

					// find out # of real palettes (check for end-of-palette markers on col[0] place):
					u32 *tintPalettePtr = (u32*)tintPalTmp;
					Assert(tintPalettePtr);
					u32 numPalettes = pTintTexture->GetHeight();
					
					for(u32 p=0; p<height; p++)
					{
						Color32 *rawCol32 = (Color32*)(tintPalettePtr + p*CCustomShaderEffectTintType::TintPaletteSize);
						const Color32 firstColor = RawColorToColor32( rawCol32[0], texColRemap );
						
						if(firstColor.GetColor() == END_OF_PALETTE_MARKER)	// end-of-palette marker on first place?
						{
							numPalettes = p;
							break;
						}
					}
					Assertf(numPalettes > 0, "%s: No tint palettes detected in texture '%s'.", CCustomShaderEffectBase_EntityName, pTintTexture->GetName());
					Assertf(numPalettes <= pTintTexture->GetHeight(), "%s: Num tint palettes exceeds height of tint texture '%s'.", CCustomShaderEffectBase_EntityName, pTintTexture->GetName());

					shaderNumPalettes[j] = numPalettes;	// store num palettes detected for this shader
				}// if(bLocked)...
	#endif //__XENON...

			}//if(effectVarTintPalTex && effectVarTintPalSelector)...
		}//for(s32 j=0; j<numShaders; j++)...

	#if TINT_PALETTE_SET_IN_EDGE
		Assert(numAllocTintShaderInfos>0);
		Assert(numAllocTintPalettes>0);
	#endif

		pType = rage_new CCustomShaderEffectTintType();

		Assert(shaderCount < 256);				// must fit into u8
		pType->m_shaderCount = (u8)shaderCount;

		Assert(numAllocTintShaderInfos < 256);	// must fit into u8
	#if TINT_PALETTE_SET_IN_EDGE
		Assert(numAllocTintPalettes < 256);		// must fit into u8
		Assert(numAllocTintDescs < 256);		// must fit into u8
		Assert(numAllocTintDescs == shaderCount);
	#endif

		pType->m_privTintInfos.Reserve(numAllocTintShaderInfos);
	#if TINT_PALETTE_SET_IN_EDGE
		pType->m_privTintPalettes.Reserve(numAllocTintPalettes);
	#endif

		u32 numGlobalTintShaderInfos= 0;
		u32 numGlobalTintPalettes	= 0;

		for(s32 j=0; j<shaderCount; j++)
		{
			grmShader		*pShader = &pShaderGroup->GetShader(j);
			Assert(pShader);

			grcEffectVar	effectVarTintPalTex		= pShader->LookupVar(VARNAME_TINT_PALETTE_TEX, FALSE);
			grcEffectVar	effectVarTintPalSelector= pShader->LookupVar(VARNAME_TINT_PAL_SELECTOR, FALSE);

			if(effectVarTintPalTex && effectVarTintPalSelector)
			{
				const bool bIsTreeShader = strstr(pShader->GetName(),"trees") != NULL;

				// find out how many palettes are stored in texture:
				const grcInstanceData& instanceData = pShader->GetInstanceData();
				const int index = (int)effectVarTintPalTex - 1;
				grcInstanceData::Entry *pEntry = &instanceData.Data()[index];
				grcTexture *pTintTexture = pEntry->Texture;
				Assertf(pTintTexture, "%s: TintPalTexture not found!", CCustomShaderEffectBase_EntityName);
				if(!pTintTexture)
					continue;

			#if __ASSERT || TINT_PALETTE_SET_IN_EDGE || __XENON
				const bool bIsWidthAppropriate	= (pTintTexture->GetWidth()==256);
				const bool bIsHeightAppropriate = (pTintTexture->GetHeight()>=4);
				const bool bIsBPPAppropriate	= (pTintTexture->GetBitsPerPixel()==32);
				artAssertf(bIsWidthAppropriate,	"%s: TintPalTexture '%s' must have width=256.", CCustomShaderEffectBase_EntityName, pTintTexture->GetName());
				artAssertf(bIsHeightAppropriate,"%s: TintPalTexture '%s' must have height>=4.", CCustomShaderEffectBase_EntityName, pTintTexture->GetName());	// ps3 & 360 min. tex height is 4
				artAssertf(bIsBPPAppropriate,	"%s: TintPalTexture '%s' must be uncompressed 32bpp (ARGB8888).", CCustomShaderEffectBase_EntityName, pTintTexture->GetName());
				if(!bIsWidthAppropriate || !bIsHeightAppropriate || !bIsBPPAppropriate)
					continue;
			#endif

	#if __ASSERT || RSG_PC || RSG_DURANGO || RSG_ORBIS
				const u32 tintPaletteTexHeight = pTintTexture->GetHeight();
	#endif

	#if TINT_PALETTE_SET_IN_EDGE
				const u32 numTintPalettes = shaderNumPalettes[j];	// ps3: real # of useful palettes for this shader
	#elif __XENON
				const u32 numTintPalettes = shaderNumPalettes[j];	// 360: real # of useful palettes for this shader
	#else
				const u32 numTintPalettes = tintPaletteTexHeight;	// 360: palette height
	#endif
				Assert(numTintPalettes>0);

				Vector2 vPaletteSelector(0,0);
				pShader->GetVar(effectVarTintPalSelector, vPaletteSelector);	// grab selected palette
				u32 nPaletteSelector = (u32)vPaletteSelector.x; 
				Assertf(nPaletteSelector<numTintPalettes,	"TintPalTexture '%s': Selected palette is too big!", pTintTexture->GetName());
				if(nPaletteSelector >= numTintPalettes)
					nPaletteSelector = numTintPalettes-1;

				structTintShaderInfo* tintInfo = &pType->m_privTintInfos.Append();	   
				numGlobalTintShaderInfos++;
		
				Assert(numTintPalettes < 256);			// must fit into u8
				Assert(nPaletteSelector < 128);			// must fit into 7 bits
	#if TINT_PALETTE_SET_IN_EDGE
				Assert(numGlobalTintPalettes < 256);	// must fit into u8
	#endif
				Assert((int)effectVarTintPalSelector < 256);	// must fit into u8
				Assert(tintPaletteTexHeight < 256);		// must fit into u8
				Assert(j < 256);						// must fit into u8

				tintInfo->m_DefaultPalette			= (u8)nPaletteSelector;		// currently selected palette
				tintInfo->m_PaletteTexHeight		= (u8)numTintPalettes;
				tintInfo->m_bIsTree					= bIsTreeShader;
				
	#if TINT_PALETTE_SET_IN_EDGE
				tintInfo->m_PaletteOffset			= (u8)numGlobalTintPalettes;// offset to first palette in big palette buffer

				// copy texture into local palette storage:
				grcTextureGCM *pTintTextureGcm = static_cast<grcTextureGCM*>(pTintTexture);
				Assert(pTintTextureGcm);
				CellGcmTexture *cellTex = pTintTextureGcm->GetTexturePtr();
				Assert(cellTex);
				Assert(cellTex->format&CELL_GCM_TEXTURE_LN);	// is texture unswizzled?

				u32 *tintPalettePtr = (u32*)pTintTextureGcm->GetBits();
				Assert(tintPalettePtr);

				for(u32 p=0; p<numTintPalettes; p++)
				{
					u32 *tintPaletteSrc = tintPalettePtr + CCustomShaderEffectTintType::TintPaletteSize*p;
					u32 *tintPaletteDst = &pType->m_privTintPalettes.Append().entries[0];
					sysMemCpy(tintPaletteDst, tintPaletteSrc, CCustomShaderEffectTintType::TintPaletteSize*sizeof(u32));
				
					// convert raw DeviceColor palette into Color32 palette
					Color32 *col32 = (Color32*)tintPaletteDst;
					for(u32 c=0; c<CCustomShaderEffectTintType::TintPaletteSize; c++)
					{
						col32[c] = RawColorToColor32( col32[c], texColRemap );
	//					Color32 c32 = col32[c];
	//					Printf("\n pal=%d: c=%d: %d %d %d %d", p, c, c32.GetRed(), c32.GetGreen(), c32.GetBlue(), c32.GetAlpha());
					}
				}// for(u32 p=0; p<numTintPalettes; p++)...
	#else	

				tintInfo->m_varPaletteTintSelector	= (u8)effectVarTintPalSelector;

	#endif	// TINT_PALETTE_SET_IN_EDGE

				tintInfo->m_ShaderIdx				= (u8)j;

				numGlobalTintPalettes += numTintPalettes;	// increase global tint palette offset
			}// if(effectVarTintPalTex && effectVarTintPalSelector)...
		
		}//for(s32 j=0; j<numShaders; j++)...
		
		// re-verify what we've just processed:
		Assert(numGlobalTintShaderInfos	== numAllocTintShaderInfos);
	#if TINT_PALETTE_SET_IN_EDGE
		Assert(numGlobalTintPalettes	== numAllocTintPalettes);
	#endif

		pType->m_bResourced = false;
		pType->m_pFrag		= NULL;
		pType->m_pDrawable	= NULL;
	}// setup tint stuff by hand...


	Assert(pType);
	pType->SetIsHighDetail(false);

	return pType;
}// end of Create()...

//
//
//
//
bool CCustomShaderEffectTintType::Initialise(rmcDrawable* UNUSED_PARAM(pDrawable))
{
	// do nothing
	return(TRUE);
}

//
//
//
//
CCustomShaderEffectTintType::~CCustomShaderEffectTintType()
{
	if(m_bResourced)
	{
		if(m_bResourcedFrag)
		{
			m_pFrag = NULL;
		}
		else
		{
			m_pDrawable = NULL;
		}
	}
}

//
//
// remove pgRef's to drawable or frag:
//
bool CCustomShaderEffectTintType::RestoreModelInfoDrawable(rmcDrawable* UNUSED_PARAM(pDrawable))
{
	if(m_bResourced)
	{
		if(m_bResourcedFrag)
		{
			m_pFrag = NULL;
		}
		else
		{
			m_pDrawable = NULL;
		}
	}

	return(true);
}

//
//
//
//
CCustomShaderEffectTintType::structTintShaderInfo* CCustomShaderEffectTintType::GetTintInfos(atUserArray<CCustomShaderEffectTintType::structTintShaderInfo>* userTintInfos)
{
	if(m_bResourced)
	{	// resourced: reconstruct tintShaderInfo from resourced frag or drawable:
		atArray<u8> *pTintData=NULL;
		if(m_bResourcedFrag)
		{
			pTintData = m_pFrag->m_pTintData;
		}
		else
		{
			pTintData = m_pDrawable->m_pTintData;
		}
		Assert(pTintData);
		Assert(pTintData->GetCount()>0);

		u8 *tintPaletteData = pTintData->GetElements();
		Assert(tintPaletteData);
		Assert16(tintPaletteData);

	#if TINT_PALETTE_SET_IN_EDGE
		const u32 tintPaletteSize	= m_privTintPalettes.GetCount() << 10;
		// omit ps3 palettes:
		u8 *tintHdrData = tintPaletteData + tintPaletteSize;
	#else
		u8 *tintHdrData = tintPaletteData;
	#endif
		tintHdrData += RscTintInfoOffset;

		if(userTintInfos)
		{
			userTintInfos->Assume((structTintShaderInfo*)tintHdrData, (u16)m_privTintInfos.GetCount());
			*(userTintInfos->GetCountPointer()) = (u16)m_privTintInfos.GetCount();
		}

		return (structTintShaderInfo*)tintHdrData;
	}
	else
	{	// non-resourced: use m_privTintInfo data directly:
		if(userTintInfos)
		{
			userTintInfos->Assume((structTintShaderInfo*)m_privTintInfos.GetElements(), (u16)m_privTintInfos.GetCapacity());
			*(userTintInfos->GetCountPointer()) = (u16)m_privTintInfos.GetCount();
		}

		return (structTintShaderInfo*)m_privTintInfos.GetElements();
	}
}

#if TINT_PALETTE_SET_IN_EDGE
CCustomShaderEffectTintType::Palette* CCustomShaderEffectTintType::GetTintPalettes(atUserArray<CCustomShaderEffectTintType::Palette>* userTintPalettes)
{
	if(m_bResourced)
	{	// resourced: reconstruct tintShaderInfo from resourced frag or drawable:
		atArray<u8> *pTintData=NULL;
		if(m_bResourcedFrag)
		{
			pTintData = m_pFrag->m_pTintData;
		}
		else
		{
			pTintData = m_pDrawable->m_pTintData;
		}
		Assert(pTintData);
		Assert(pTintData->GetCount()>0);

		u8 *tintPaletteData = pTintData->GetElements();
		Assert(tintPaletteData);
		Assert16(tintPaletteData);

		if(userTintPalettes)
		{
			userTintPalettes->Assume((Palette*)tintPaletteData, (u16)m_privTintPalettes.GetCount());
			*(userTintPalettes->GetCountPointer()) = (u16)m_privTintPalettes.GetCount();
		}

		return (Palette*)tintPaletteData;
	}
	else
	{	// non-resourced: use m_privTintInfo data directly:
		if(userTintPalettes)
		{
			userTintPalettes->Assume((Palette*)m_privTintPalettes.GetElements(), (u16)m_privTintPalettes.GetCapacity());
			*(userTintPalettes->GetCountPointer()) = (u16)m_privTintPalettes.GetCount();
		}

		return (Palette*)m_privTintPalettes.GetElements();
	}
}
#endif //TINT_PALETTE_SET_IN_EDGE...


//
//
//
// instance create in CEntity::CreateDrawable()
//
CCustomShaderEffectBase*	CCustomShaderEffectTintType::CreateInstance(CEntity* pEntity)
{
	const u32 currentPaletteBytes	= (m_privTintInfos.GetCount() + 15) & ~15;
	u32	nSize = sizeof(CCustomShaderEffectTint) + currentPaletteBytes;
#if TINT_PALETTE_SET_IN_EDGE
	const u32 tintDescSize			= ((m_shaderCount + 3) & ~3) * sizeof(u32);
	nSize += tintDescSize;
#endif
	CCustomShaderEffectTint* pNewCseTint= rage_placement_new(rage_aligned_new(16) u8[nSize]) CCustomShaderEffectTint( nSize );

	pNewCseTint->m_pType				= this;

#if TINT_PALETTE_SET_IN_EDGE
	pNewCseTint->m_currentPaletteBytes	= currentPaletteBytes;

	// Reset them all to zero because some shaders might not otherwise have entries.
	for(u32 i=0; i<m_shaderCount; i++)
	{
		*pNewCseTint->GetTintDesc(i) = 0x00000000;
	}
#endif

	pNewCseTint->SelectTintPalette((u8)pEntity->GetTintIndex(), pEntity);	// 0xff = select default tint palette per tint info

	return(pNewCseTint);
}// end of CreateInstance()...



//
//
//
//
CCustomShaderEffectTint::CCustomShaderEffectTint(u32 size) : CCustomShaderEffectBase(size, CSE_TINT)
{
}

//
//
//
//
CCustomShaderEffectTint::~CCustomShaderEffectTint()
{
}



//
//
// returns maximum palette allowed for this tint instance:
//
u32	CCustomShaderEffectTint::GetMaxTintPalette()
{
	u32 maxPal=0;

	atUserArray<CCustomShaderEffectTintType::structTintShaderInfo> tintInfos;
	this->m_pType->GetTintInfos(&tintInfos);

	const u32 numTintShaderInfos = tintInfos.GetCount();
	for(u32 t=0; t<numTintShaderInfos; t++)
	{
		CCustomShaderEffectTintType::structTintShaderInfo* tintInfo = &tintInfos[t];

		if(tintInfo->m_PaletteTexHeight > maxPal)
		{
			maxPal = tintInfo->m_PaletteTexHeight;
		}
	}

	return(maxPal);
}

//
//
// returns amount of all tint palettes:
//
u32	CCustomShaderEffectTint::GetNumTintPalettes()
{
u32 numPal=0;

	atUserArray<CCustomShaderEffectTintType::structTintShaderInfo> tintInfos;
	this->m_pType->GetTintInfos(&tintInfos);

	const u32 numTintShaderInfos = tintInfos.GetCount();
	for(u32 t=0; t<numTintShaderInfos; t++)
	{
		CCustomShaderEffectTintType::structTintShaderInfo* tintInfo = &tintInfos[t];
		numPal += tintInfo->m_PaletteTexHeight;
	}

	return(numPal);
}

//
//
// selects given or default (=0xff) tint palette:
//
void CCustomShaderEffectTint::SelectTintPalette(u8 selpal, fwEntity* pEntity)
{
	SelectTintPalette(selpal, m_currentPalette, m_pType, pEntity);
}

void CCustomShaderEffectTint::SelectTintPalette(u8 selpal, u8 *result, CCustomShaderEffectTintType *type, fwEntity* ASSERT_ONLY(pEntity))
{
	atUserArray<CCustomShaderEffectTintType::structTintShaderInfo> tintInfos;
	type->GetTintInfos(&tintInfos);

	const u32 numTintShaderInfos = tintInfos.GetCount();
	for(u32 t=0; t<numTintShaderInfos; t++)
	{
		CCustomShaderEffectTintType::structTintShaderInfo* tintInfo = &tintInfos[t];

		u8 pal = (selpal==0xff? tintInfo->m_DefaultPalette : selpal);
		if(pal >= tintInfo->m_PaletteTexHeight)
		{
		#if __ASSERT
			bool bDisplayWarning = true;
			#if CSE_TINT_EDITABLEVALUES
				if(ms_bEVEnableForceTintPalette)
				{
					bDisplayWarning = false;	// don't display warning messages when RAG's tint selection override is in use
				}
			#endif

			if(pEntity)
			{
				const fwArchetype *pArchetype = pEntity->GetArchetype();
				const char* entityName = pArchetype->GetModelName();
				if(bDisplayWarning)
					renderWarningf("CSETint: %s: Selected palette %d is too high (max: %d).", entityName, pal, tintInfo->m_PaletteTexHeight-1);
			}
			else
			{
				if(bDisplayWarning)
					renderWarningf("CSETint: Selected palette %d is too high (max: %d).", pal, tintInfo->m_PaletteTexHeight-1);
			}
		#endif //__ASSERT...

			pal = tintInfo->m_PaletteTexHeight-1;
		}

		result[t] = pal;
	}

}// end of SelectTintPalette()...


//
//
//
// instance update in CEntity::PreRender()
//
void CCustomShaderEffectTint::Update(fwEntity* BANK_ONLY(pEntity))
{
	Assert(pEntity);

#if CSE_TINT_EDITABLEVALUES
	if(ms_bEVEnableForceTintPalette)
	{
		SelectTintPalette((u8)ms_nEVForcedTintPalette, pEntity);
	}
#endif
} // end of Update()...

//
//
//
// instance setting variables in CEntity/DynamicEntity::Render()
//
void CCustomShaderEffectTint::SetShaderVariables(rmcDrawable* pDrawable)
{
	SetShaderVariables(m_currentPalette, m_pType, pDrawable);
}// end of SetShaderVariables()...

void CCustomShaderEffectTint::SetShaderVariables(u8 *selPal, CCustomShaderEffectTintType *type, rmcDrawable* pDrawable)
{
#if __BANK
	if(PARAM_tintdisable.Get())
		return;
#endif

	if(!pDrawable)
		return;

	if(DRAWLISTMGR->IsExecutingShadowDrawList() || grmModel::GetForceShader())
		return;
#if __BANK
	if(DRAWLISTMGR->IsExecutingDebugOverlayDrawList())
		return;
#endif


	atUserArray<CCustomShaderEffectTintType::structTintShaderInfo> tintInfos;
	type->GetTintInfos(&tintInfos);

#if TINT_PALETTE_SET_IN_EDGE
	// recreate tint desc pointers (resourced tint palettes may have moved due to defragmentation):

	atUserArray<CCustomShaderEffectTintType::Palette> tintPalettes;
	m_pType->GetTintPalettes(&tintPalettes);

	const u32 numTintShaderInfos = tintInfos.GetCount();
	for(u32 t=0; t<numTintShaderInfos; t++)
	{
		CCustomShaderEffectTintType::structTintShaderInfo* tintInfo = &tintInfos[t];
		const u8 pal = m_currentPalette[t];

		// recreate tintDescs:
		u32 *tintDesc = this->GetTintDesc(tintInfo->m_ShaderIdx);

		// grab palette from parent:
		u32* palPtr = &tintPalettes[ pal + tintInfo->m_PaletteOffset ].entries[0];

		Assert16(palPtr);
		*tintDesc = (u32)palPtr | u32(tintInfo->m_bIsTree);	// EDGE: set lowest bit for special tint unpack
	}


	SPU_COMMAND(GTA4__SetTintDescriptor,0);
	cmd->tintDescriptorPtr		= this->GetTintDesc(0);
	Assert16(cmd->tintDescriptorPtr);
	cmd->tintDescriptorCount	= this->m_pType->GetShaderCount();
	Assert(cmd->tintDescriptorCount > 0);
	cmd->tintShaderIdx			= 0x00;

#else //TINT_PALETTE_SET_IN_EDGE...

	const u32 numTintShaderInfos = tintInfos.GetCount();
	for(u32 t=0; t<numTintShaderInfos; t++)
	{
		CCustomShaderEffectTintType::structTintShaderInfo* tintInfo = &tintInfos[t];

	#if __XENON
		// 64x4H tint palettes
		u32 texHeightQuater = (tintInfo->m_PaletteTexHeight + 3) & (~0x3);	// roundup to nearest multiple of 4 for real 360 texture quater height
		u32 texHeight		= texHeightQuater * 4;							// real texture height for 64x4H
		Vector2 palSelect(float(selPal[t]) / float(texHeightQuater), 1.0f/float(texHeight));
		pDrawable->GetShaderGroup().GetShader(tintInfo->m_ShaderIdx).SetVar((grcEffectVar)tintInfo->m_varPaletteTintSelector, palSelect);
	#else
		// 256xH tint palettes
		u32 texHeight		= tintInfo->m_PaletteTexHeight;
		Vector2 palSelect(float(selPal[t]) / float(texHeight), 0.0f);
		pDrawable->GetShaderGroup().GetShader(tintInfo->m_ShaderIdx).SetVar((grcEffectVar)tintInfo->m_varPaletteTintSelector, palSelect);
	#endif
	}

#endif //TINT_PALETTE_SET_IN_EDGE...

}// end of SetShaderVariables()...

//
//
//
//
void CCustomShaderEffectTint::AddToDrawList(u32 modelIndex, bool bExecute)
{
	// no tint DLC for shadows DL:
	if(DRAWLISTMGR->IsBuildingShadowDrawList())
		return;
		
	if(DRAWLISTMGR->IsBuildingRainCollisionMapDrawList())
		return;

	DLC(CCustomShaderEffectDC, (*this, modelIndex, bExecute, m_pType));
}

//
//
//
//
#if TINT_PALETTE_SET_IN_EDGE
static void ResetEdgeTintVariables()
{
	SPU_COMMAND(GTA4__SetTintDescriptor,0);
	cmd->tintDescriptorPtr		= NULL;
	cmd->tintDescriptorCount	= 0;
	cmd->tintShaderIdx			= 0x00;
}
#endif

void CCustomShaderEffectTint::AddToDrawListAfterDraw()
{
#if TINT_PALETTE_SET_IN_EDGE
	// no tint DLC for shadows DL:
	if(DRAWLISTMGR->IsBuildingShadowDrawList())
		return;
		
	if(DRAWLISTMGR->IsBuildingRainCollisionMapDrawList())
		return;

	DLC_Add( ResetEdgeTintVariables );
#endif
}

u32 CCustomShaderEffectTint::AddDataForPrototype(void * address)
{
	u32 size = 0;
	DrawListAddress drawListOffset;

	dlDrawListInfo *drawListInfo = DRAWLISTMGR->GetBuildExecDLInfo();
	if(!drawListInfo || !CDrawListMgr::IsShadowDrawList(drawListInfo->m_type))
	{
#if __PS3
		CopyOffShader(this, this->Size(), drawListOffset, true, m_pType);
#else
		CopyOffShader(this, this->Size(), drawListOffset, false, m_pType);
#endif
	}

	*static_cast<DrawListAddress*>(address) = drawListOffset;
	size += sizeof(DrawListAddress);
	return size;
}

#if CSE_TINT_EDITABLEVALUES
//
//
//
//
bool	CCustomShaderEffectTint::ms_bEVEnableForceTintPalette	= FALSE;
u32		CCustomShaderEffectTint::ms_nEVForcedTintPalette		= 0;

bool CCustomShaderEffectTint::InitWidgets(bkBank& bank)
{
	bank.PushGroup("Mesh Palette Tinting", false);
		bank.AddToggle("Force Tint Palette",	&ms_bEVEnableForceTintPalette);
		bank.AddSlider("Tint Palette Selector",	&ms_nEVForcedTintPalette, 0, 255, 1);
	bank.PopGroup();

	return(TRUE);
}
#endif //CSE_TINT_EDITABLEVALUES...




#if TINT_PALETTE_SET_IN_EDGE

#if __ASSERT
	// Test if a number is a power of two.
	static inline bool _isPowerOfTwo(unsigned int value)
	{
		return (value & (value-1)) == 0;
	}
#endif

// Interleave lower 16 bits with 0s.  Ie: 1111 becomes 01010101
static inline unsigned int _spreadBits(unsigned int bits)
{
	bits &= 0xFFFF;

	bits = (bits | (bits<<8)) & 0x00FF00FF;
	bits = (bits | (bits<<4)) & 0x0F0F0F0F;
	bits = (bits | (bits<<2)) & 0x33333333;
	bits = (bits | (bits<<1)) & 0x55555555;

	return bits;
}

// Pack alternate bits into the lower 16 bits of the result.  Ie: x1x1x1x1 becomes 1111.
static inline unsigned int _packBits(unsigned int bits)
{
	bits &= 0x55555555;

	bits = (bits | (bits >> 1)) & 0x33333333;
	bits = (bits | (bits >> 2)) & 0x0F0F0F0F;
	bits = (bits | (bits >> 4)) & 0x00FF00FF;
	bits = (bits | (bits >> 8)) & 0x0000FFFF;

	return bits;
}


// Unswizzle the texture data.
template <typename TEXELTYPE>
void unswizzleImage(TEXELTYPE *dst, const TEXELTYPE *src, unsigned int width, unsigned int height)
{
	Assert(dst);
	Assert(src);
	Assert(_isPowerOfTwo(width));
	Assert(_isPowerOfTwo(height));

	// X coordinate occupies the even bits, Y coordinate occupies the odd bits:
	// yxyxyxyxyxyxyxyxyxyx
	const u32 xAdd = (width > height) ? 1 : 0;
	const u32 yAdd = 1-xAdd;
	const u32 overflowMask = _spreadBits( MIN(width,height) ) << xAdd;

	u32 pixelsLeft = width*height;
	u32 curPos = 0;
	u32 curX=0;
	u32 curY=0;
	while (pixelsLeft--)
	{
		if (overflowMask & curPos)
		{
			// Can't go more in this direction, so move right or down along the Z (non-square textures)
			curX += xAdd;
			curX &= ~(xAdd-1);

			curY += yAdd;
			curY &= ~(yAdd-1);

			curPos = _spreadBits(curX) | (_spreadBits(curY) << 1);
		}

		// Extract the current bit positions for the x and y coordinates
		curX = _packBits(curPos);
		curY = _packBits(curPos >> 1);

		// Write the next pixel in the unswizzled image
		dst[curY*width + curX] = *src++;

		// Go to the next spot in our scanning order
		curPos++;
	}
}

//
//
// Unswizzle texture data back to a linear format.
//
bool CCustomShaderEffectTintType::UnswizzleTextureData(void *dstPixels, const void *srcPixels, u32 bpp, u32 width, u32 height)
{
	Assert(srcPixels);
	Assert(dstPixels);
	// Textures to be unswizzled must be power of 2 in each dimension
	Assert(_isPowerOfTwo(width));
	Assert(_isPowerOfTwo(height));

	switch (bpp)	// bits-per-pixel
	{
	case 8:
		unswizzleImage<u8>((u8*)dstPixels, (const u8*)srcPixels, width, height);
		break;
	case 16:
		unswizzleImage<u16>((u16*)dstPixels, (const u16*)srcPixels, width, height);
		break;
	case 128:
		width *= 2;
		// Fall thru.
	case 64:
		width *= 2;
		// Fall thru.
	case 32:
		unswizzleImage<u32>((u32*)dstPixels, (const u32*)srcPixels, width, height);
		break;
	default:
		Assertf(0, "Invalid bpp");
		return(FALSE);
	}

	return(TRUE);
}

#endif //TINT_PALETTE_SET_IN_EDGE...

//
// convert raw color from tint palette into 'proper' Color32:
//
inline Color32 RawColorToColor32(Color32 colorDC, grcTexture::eTextureSwizzle * /*remap*/)
{
// remap is broken for tint textures:
// 360/ps3 runtime: tint textures have order ARGB in memory, but remap is BGRA (360) or RGBA (ps3)
// resourcing:      tint textures have order BGRA in memory, but remap is BGRA (360) or RGBA (ps3)
#if 0
u8 dst[4] = {0, 0, 0, 0};

	for(u32 i=0; i<4; i++) // r,g,b,a
	{
		switch(remap[i])
		{
		case grcTexture::TEXTURE_SWIZZLE_R: dst[i] = colorDC.GetRed  ();	break;
		case grcTexture::TEXTURE_SWIZZLE_G: dst[i] = colorDC.GetGreen();	break;
		case grcTexture::TEXTURE_SWIZZLE_B: dst[i] = colorDC.GetBlue ();	break;
		case grcTexture::TEXTURE_SWIZZLE_A: dst[i] = colorDC.GetAlpha();	break;
		case grcTexture::TEXTURE_SWIZZLE_0: dst[i] = 0x00;					break;
		case grcTexture::TEXTURE_SWIZZLE_1: dst[i] = 0xff;					break;
		}
	}

	return Color32(dst[0], dst[1], dst[2], dst[3]);
#else
	#if __RESOURCECOMPILER
		const bool bResourcingPs3				= (RageBuilder::GetPlatform()==RageBuilder::pltPs3);
		const bool bResourcingXenon				= (RageBuilder::GetPlatform()==RageBuilder::pltXenon);

		if(bResourcingPs3 || bResourcingXenon)
		{
			u8 src[4] = {0, 0, 0, 0};
			src[0] = colorDC.GetAlpha();	// byte 0
			src[1] = colorDC.GetRed();		// byte 1
			src[2] = colorDC.GetGreen();	// byte 2
			src[3] = colorDC.GetBlue();		// byte 4
			return Color32(src[2], src[1], src[0], src[3]);	// r,g,b,a
		}
		else
		{
			return colorDC;
		}
	#else //__RESOURCECOMPILER
		return colorDC;	// color field order is already correct
	#endif
#endif //#if 0...
}
