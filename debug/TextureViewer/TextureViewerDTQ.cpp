// ========================================
// debug/textureviewer/textureviewerDTQ.cpp
// (c) 2010 RockstarNorth
// ========================================

#include "debug/textureviewer/textureviewerprivate.h"

#if __BANK

// Rage headers
#include "file/asset.h"
#include "grcore/image.h"
#include "grcore/texture.h"
#include "grcore/texture_d3d11.h"
#include "grcore/texture_durango.h"
#include "grcore/texture_gnm.h"
#include "grcore/stateblock.h"
#include "grmodel/shader.h"
#include "system/memops.h"

// Framework headers
#include "fwutil/xmacro.h"
#include "fwmaths/rect.h"

// Game headers
#include "core/game.h"
#include "renderer/RenderPhases/RenderPhaseReflection.h"

#include "debug/textureviewer/textureviewerDTQ.h"

DEBUG_TEXTURE_VIEWER_OPTIMISATIONS()

// ================================================================================================

namespace DebugTextureViewerDTQ { // DTQ stands for "Draw Textured Quad"

const char** GetUIStrings_eDTQViewMode()
{
	static const char* strings[] =
	{
		STRING(RGB         ),
		STRING(RGB + A     ),
		STRING(R+G+B+A     ),
		STRING(RGBA        ),
		STRING(R           ),
		STRING(G           ),
		STRING(B           ),
		STRING(R_monochrome),
		STRING(G_monochrome),
		STRING(B_monochrome),
		STRING(A_monochrome),
		STRING(RGB_invert_A),
		STRING(specular    ),
	};
	return strings;
}

static void GetColourMatrix(Vector4 colourMatrix[5], eDTQViewMode viewMode)
{
	sysMemSet(colourMatrix, 0, 5*sizeof(Vector4));

	switch ((int)viewMode)
	{
	case eDTQVM_RGB:
	case eDTQVM_RGBA:
	case eDTQVM_RGB_plus_A:
	case eDTQVM_R_G_B_A:
		colourMatrix[0].x = 1.0f;
		colourMatrix[1].y = 1.0f;
		colourMatrix[2].z = 1.0f;
		colourMatrix[viewMode == eDTQVM_RGB ? 4 : 3].w = 1.0f;
		break;
	case eDTQVM_R:
		colourMatrix[0].x = 1.0f;
		colourMatrix[4].w = 1.0f;
		break;
	case eDTQVM_G:
		colourMatrix[1].y = 1.0f;
		colourMatrix[4].w = 1.0f;
		break;
	case eDTQVM_B:
		colourMatrix[2].z = 1.0f;
		colourMatrix[4].w = 1.0f;
		break;
	case eDTQVM_R_monochrome:
		colourMatrix[0].x = 1.0f;
		colourMatrix[1].x = 1.0f;
		colourMatrix[2].x = 1.0f;
		colourMatrix[4].w = 1.0f;
		break;
	case eDTQVM_G_monochrome:
		colourMatrix[0].y = 1.0f;
		colourMatrix[1].y = 1.0f;
		colourMatrix[2].y = 1.0f;
		colourMatrix[4].w = 1.0f;
		break;
	case eDTQVM_B_monochrome:
		colourMatrix[0].z = 1.0f;
		colourMatrix[1].z = 1.0f;
		colourMatrix[2].z = 1.0f;
		colourMatrix[4].w = 1.0f;
		break;
	case eDTQVM_A_monochrome:
		colourMatrix[0].w = 1.0f;
		colourMatrix[1].w = 1.0f;
		colourMatrix[2].w = 1.0f;
		colourMatrix[4].w = 1.0f;
		break;
	case eDTQVM_RGB_invert_A:
		colourMatrix[0].x = 1.0f;
		colourMatrix[1].y = 1.0f;
		colourMatrix[2].z = 1.0f;
		colourMatrix[3].w = -1.0f;
		colourMatrix[4].w = 1.0f;
		break;
	case eDTQVM_specular:
		break;
	}
}

static grmShader*         g_shader                   = NULL;
static grcEffectTechnique g_technique                = grcetNONE;
static grcEffectVar       g_texture2D                = grcevNONE;
static grcEffectVar       g_texture2DFiltered        = grcevNONE;
static grcEffectVar       g_texture3D                = grcevNONE;
static grcEffectVar       g_texture3DFiltered        = grcevNONE;
static grcEffectVar       g_textureCube              = grcevNONE;
static grcEffectVar       g_textureCubeFiltered      = grcevNONE;
static grcEffectVar       g_texture2DArray           = grcevNONE;
static grcEffectVar       g_texture2DArrayFiltered   = grcevNONE;
static grcEffectVar       g_textureCubeArray         = grcevNONE;
static grcEffectVar       g_textureCubeArrayFiltered = grcevNONE;
static grcEffectVar       g_params[9]                = {grcevNONE,grcevNONE,grcevNONE,grcevNONE,grcevNONE,grcevNONE,grcevNONE,grcevNONE,grcevNONE};
static grcEffectVar       g_cubemapRotation          = grcevNONE;  

static grcDepthStencilStateHandle g_stateD = grcStateBlock::DSS_Invalid;
static grcBlendStateHandle        g_stateB = grcStateBlock::BS_Invalid;
static grcRasterizerStateHandle   g_stateR = grcStateBlock::RS_Invalid;

void DTQInit()
{
	static bool bOnce = true;

	if (bOnce)
	{
		bOnce = false;

		const char* shaderName = "debug_DTQ";

		ASSET.PushFolder("common:/shaders");
		g_shader = grmShaderFactory::GetInstance().Create();
		Assert(g_shader);

		if (g_shader && g_shader->Load(shaderName, NULL, false))
		{
			g_technique                = g_shader->LookupTechnique("DTQ");
			g_texture2D                = g_shader->LookupVar("previewTexture2D");
			g_texture2DFiltered        = g_shader->LookupVar("previewTexture2DFiltered");
			g_texture3D                = g_shader->LookupVar("previewTexture3D");
			g_texture3DFiltered        = g_shader->LookupVar("previewTexture3DFiltered");
			g_textureCube              = g_shader->LookupVar("previewTextureCube");
			g_textureCubeFiltered      = g_shader->LookupVar("previewTextureCubeFiltered");
			g_texture2DArray           = g_shader->LookupVar("previewTexture2DArray");
			g_texture2DArrayFiltered   = g_shader->LookupVar("previewTexture2DArrayFiltered");
			g_textureCubeArray         = g_shader->LookupVar("previewTextureCubeArray");
			g_textureCubeArrayFiltered = g_shader->LookupVar("previewTextureCubeArrayFiltered");
			g_params[0]                = g_shader->LookupVar("param0");
			g_params[1]                = g_shader->LookupVar("param1");
			g_params[2]                = g_shader->LookupVar("param2");
			g_params[3]                = g_shader->LookupVar("param3");
			g_params[4]                = g_shader->LookupVar("param4");
			g_params[5]                = g_shader->LookupVar("param5");
			g_params[6]                = g_shader->LookupVar("param6");
			g_params[7]                = g_shader->LookupVar("param7");
			g_params[8]                = g_shader->LookupVar("param8");
			g_cubemapRotation          = g_shader->LookupVar("cubemapRotation");
		}
		else
		{
			Assertf(false, "Failed to load \"%s\", try rebuilding the shader or consider bumping c_MaxEffects in effect.h!", shaderName);
			g_shader = NULL;
		}

		grcDepthStencilStateDesc descD;
		grcBlendStateDesc        descB;
		grcRasterizerStateDesc   descR;

		descD.DepthEnable                = false;
		descD.DepthWriteMask             = false;
		descD.DepthFunc					 = grcRSV::CMP_LESS;
		descB.BlendRTDesc[0].BlendEnable = true;
		descB.BlendRTDesc[0].SrcBlend    = grcRSV::BLEND_INVSRCALPHA; // swapped blend states intentionally as a test .. compensated for in debug_DTQ.fx
		descB.BlendRTDesc[0].DestBlend   = grcRSV::BLEND_SRCALPHA;
		descR.CullMode                   = grcRSV::CULL_NONE;

		g_stateD = grcStateBlock::CreateDepthStencilState(descD);
		g_stateB = grcStateBlock::CreateBlendState       (descB);
		g_stateR = grcStateBlock::CreateRasterizerState  (descR);

		ASSET.PopFolder();
	}
}

static void DTQSetRenderState()
{
	grcStateBlock::SetDepthStencilState(g_stateD);
	grcStateBlock::SetBlendState       (g_stateB);
	grcStateBlock::SetRasterizerState  (g_stateR);
}

void DTQBlitSolidColour(
	eDTQPixelCoordSpace pixelCoordSpace,
	float               x1,
	float               y1,
	float               x2,
	float               y2,
	float               z,
	const Color32&      colour,
	bool                bSolid
	)
{
#if __PS3 || __XENON
	DTQInit();
#endif // __PS3 || __XENON

	if (g_shader == NULL)
	{
		return;
	}

	if (pixelCoordSpace == eDTQPCS_Pixels)
	{
		const float sx = 2.0f/(float)GRCDEVICE.GetWidth ();
		const float sy = 2.0f/(float)GRCDEVICE.GetHeight();

		x1 = +(sx*x1 - 1.0f);
		y1 = -(sy*y1 - 1.0f);
		x2 = +(sx*x2 - 1.0f);
		y2 = -(sy*y2 - 1.0f);
	}
	else if (pixelCoordSpace == eDTQPCS_NormalisedZeroToOne)
	{
		x1 = x1*2.0f - 1.0f;
		y1 = y1*2.0f - 1.0f;
		x2 = x2*2.0f - 1.0f;
		y2 = y2*2.0f - 1.0f;
	}
	else if (pixelCoordSpace == eDTQPCS_Normalised)
	{
		// no change
	}
	else
	{
		return;
	}

	DTQSetRenderState();
	AssertVerify(g_shader->BeginDraw(grmShader::RMC_DRAW, false, g_technique));

	g_shader->Bind(1); // PS_DTQ_NoTexture
	{
		if (bSolid)
		{
			grcBegin(drawTriStrip, 4);
			grcVertex(x1, y1, z, 0.0f, 0.0f, -1.0f, colour, 0.0f, 0.0f);
			grcVertex(x1, y2, z, 0.0f, 0.0f, -1.0f, colour, 0.0f, 0.0f);
			grcVertex(x2, y1, z, 0.0f, 0.0f, -1.0f, colour, 0.0f, 0.0f);
			grcVertex(x2, y2, z, 0.0f, 0.0f, -1.0f, colour, 0.0f, 0.0f);
			grcEnd();
		}
		else
		{
			grcBegin(drawLineStrip, 5);
			grcVertex(x1, y1, z, 0.0f, 0.0f, -1.0f, colour, 0.0f, 0.0f);
			grcVertex(x1, y2, z, 0.0f, 0.0f, -1.0f, colour, 0.0f, 0.0f);
			grcVertex(x2, y2, z, 0.0f, 0.0f, -1.0f, colour, 0.0f, 0.0f);
			grcVertex(x2, y1, z, 0.0f, 0.0f, -1.0f, colour, 0.0f, 0.0f);
			grcVertex(x1, y1, z, 0.0f, 0.0f, -1.0f, colour, 0.0f, 0.0f); // close loop
			grcEnd();
		}
	}
	g_shader->UnBind();
	g_shader->EndDraw();
}

void DTQBlit(
	eDTQViewMode        viewMode          ,
	bool                bFiltered         ,
	float               LOD               ,
	int                 layer             ,
	float               volumeSlice       ,
	int                 volumeCoordSwap   ,
	bool                cubeForceOpaque   ,
	bool                cubeSampleAllMips ,
	float               cubeCylinderAspect,
	float               cubeFaceZoom      ,
	eDTQCubeFace        cubeFace          ,
	Vec3V_In            cubeRotationYPR   ,
	eDTQCubeProjection  cubeProjection    ,
	bool                cubeProjectionClip,
	bool                cubeNegate        ,
	float               cubeRefDots       ,
	const Vector3&      specularL         ,
	float               specularBumpiness ,
	float               specularExponent  ,
	float               specularScale     ,
	eDTQPixelCoordSpace pixelCoordSpace   ,
	const fwBox2D&      dstBounds         ,
	float               dstZ              ,
	eDTQTexelCoordSpace texelCoordSpace   ,
	const fwBox2D&      srcBounds         ,
	const Color32&      colour            ,
	float               brightness        ,
	const grcTexture*   texture
	)
{
#if __PS3 || __XENON
	DTQInit();
#endif // __PS3 || __XENON

	if (g_shader == NULL)
	{
		return;
	}

	grcImage::ImageType imageType = grcImage::STANDARD;

	if (dynamic_cast<const grcRenderTarget*>(texture))
	{
		if (texture == CRenderPhaseReflection::ms_renderTargetCube)
		{
			imageType = grcImage::CUBE;
		}
		else
		{
			// TODO -- support render targets
		}
	}
	else
	{
#if RSG_ORBIS
		imageType = (grcImage::ImageType)static_cast<const grcTextureGNM*>(texture)->GetImageType();
#elif RSG_DURANGO
		imageType = (grcImage::ImageType)static_cast<const grcTextureDurango*>(texture)->GetImageType();
#elif RSG_PC
		imageType = (grcImage::ImageType)static_cast<const grcTextureDX11*>(texture)->GetImageType();
#else
		#error 1 && "Unknown Platform";
#endif
	}

	eDTQTextureType type = eDTQTT_2D;

	if (imageType == grcImage::STANDARD)
	{
		if (texture->GetLayerCount() == 1) { type = eDTQTT_2D; }
		else                               { type = eDTQTT_2DArray; }
	}
	else if (imageType == grcImage::VOLUME)
	{
		if (texture->GetLayerCount() == 1) { type = eDTQTT_3D; }
		else                               { type = eDTQTT_3DArray; }
	}
	else if (imageType == grcImage::CUBE)
	{
		if (texture->GetLayerCount() == 6) { type = eDTQTT_Cube; }
		else                               { type = eDTQTT_CubeArray; }
	}

	float x1 = dstBounds.x0;
	float y1 = dstBounds.y0;
	float x2 = dstBounds.x1;
	float y2 = dstBounds.y1;
	float z  = dstZ;

	float u1 = srcBounds.x0;
	float v1 = srcBounds.y0;
	float u2 = srcBounds.x1;
	float v2 = srcBounds.y1;

	Vector4 colourMatrix[5];
	GetColourMatrix(colourMatrix, viewMode);

	float cubeZoomOrVolumeCoordSwap = 0.0f;

	if (imageType == grcImage::CUBE)
	{
		cubeZoomOrVolumeCoordSwap = cubeProjection ? -(float)cubeProjection : cubeFaceZoom;

		if (cubeProjection && cubeProjectionClip)
		{
			cubeZoomOrVolumeCoordSwap -= 0.5f;
		}
	}
	else if (imageType == grcImage::VOLUME)
	{
		cubeZoomOrVolumeCoordSwap = (float)volumeCoordSwap;
	}

	g_shader->SetVar(g_params[0], colourMatrix[0]*brightness);
	g_shader->SetVar(g_params[1], colourMatrix[1]*brightness);
	g_shader->SetVar(g_params[2], colourMatrix[2]*brightness);
	g_shader->SetVar(g_params[3], colourMatrix[3]);
	g_shader->SetVar(g_params[4], colourMatrix[4]);
	g_shader->SetVar(g_params[5], Vec4f(
		(imageType == grcImage::CUBE && cubeSampleAllMips) ? -1.0f : LOD,
		(float)type,
		(float)layer + ((imageType == grcImage::CUBE && cubeForceOpaque) ? 0.5f : 0.0f),
		volumeSlice
	));
	g_shader->SetVar(g_params[6], Vec4f(
		specularL.x,
		specularL.y,
		specularL.z,
		specularBumpiness
	));
	g_shader->SetVar(g_params[7], Vec4f(
		specularExponent,
		specularScale,
		cubeZoomOrVolumeCoordSwap, 
		cubeRefDots
	));
	g_shader->SetVar(g_params[8], Vec4f(
		1.0f, // cylinder radius
		2.0f*PI/cubeCylinderAspect,
		0.0f,
		0.0f
	));

	if (type == eDTQTT_Cube || type == eDTQTT_CubeArray)
	{
		float yaw   = cubeRotationYPR.GetXf();
		float pitch = cubeRotationYPR.GetYf();
		float roll  = cubeRotationYPR.GetZf();

		switch ((int)cubeFace)
		{
		case eDTQCF_PZ_UP:    pitch -= 90.0f; break;
		case eDTQCF_NZ_DOWN:  pitch += 90.0f; break;
		case eDTQCF_NX_LEFT:  yaw += 90.0f; break;
		case eDTQCF_PX_RIGHT: yaw -= 90.0f; break;
		case eDTQCF_PY_FRONT: break;
		case eDTQCF_NY_BACK:  yaw += 180.0f; break;
		}

		Mat34V rotation;
		Mat34VFromEulersXYZ(rotation, Vec3V(pitch, roll, -yaw)*ScalarV(V_TO_RADIANS));

		if (cubeNegate)
		{
			rotation.GetCol0Ref() *= ScalarV(V_NEGONE);
			rotation.GetCol1Ref() *= ScalarV(V_NEGONE);
			rotation.GetCol2Ref() *= ScalarV(V_NEGONE);
		}

		g_shader->SetVar(g_cubemapRotation, rotation);
	}

	int pass = bFiltered ? 2 : 0; // PS_DTQ_Filtered, PS_DTQ

	if (viewMode == eDTQVM_specular)
	{
		pass = bFiltered ? 4 : 3; // PS_DTQ_FilteredSpecular, PS_DTQ_Specular
	}

	grcEffectVar var = grcevNONE;

	if (texture)
	{
		if (bFiltered)
		{
			switch (type)
			{
			case eDTQTT_2D       : var = g_texture2DFiltered; break;
			case eDTQTT_3D       : var = g_texture3DFiltered; break;
			case eDTQTT_Cube     : var = g_textureCubeFiltered; break;
			case eDTQTT_2DArray  : var = g_texture2DArrayFiltered; break;
			case eDTQTT_3DArray  : break;
			case eDTQTT_CubeArray: var = g_textureCubeArrayFiltered; break;
			}
		}
		else
		{
			switch (type)
			{
			case eDTQTT_2D       : var = g_texture2D; break;
			case eDTQTT_3D       : var = g_texture3D; break;
			case eDTQTT_Cube     : var = g_textureCube; break;
			case eDTQTT_2DArray  : var = g_texture2DArray; break;
			case eDTQTT_3DArray  : break;
			case eDTQTT_CubeArray: var = g_textureCubeArray; break;
			}
		}

		g_shader->SetVar(var, texture);

		if (texelCoordSpace == eDTQTCS_Texels)
		{
			const float oow = 1.0f/texture->GetWidth ();
			const float ooh = 1.0f/texture->GetHeight();

			u1 *= oow;
			v1 *= ooh;
			u2 *= oow;
			v2 *= ooh;
		}
	}

	// set other texture vars, so shader doesn't complain ..
	if (var != g_texture2DFiltered       ) { g_shader->SetVar(g_texture2DFiltered       , grcTexture::None); }
	if (var != g_texture3DFiltered       ) { g_shader->SetVar(g_texture3DFiltered       , grcTexture::None); }
	if (var != g_textureCubeFiltered     ) { g_shader->SetVar(g_textureCubeFiltered     , grcTexture::None); }
	if (var != g_texture2DArrayFiltered  ) { g_shader->SetVar(g_texture2DArrayFiltered  , grcTexture::None); }
	if (var != g_textureCubeArrayFiltered) { g_shader->SetVar(g_textureCubeArrayFiltered, grcTexture::None); }
	if (var != g_texture2D               ) { g_shader->SetVar(g_texture2D               , grcTexture::None); }
	if (var != g_texture3D               ) { g_shader->SetVar(g_texture3D               , grcTexture::None); }
	if (var != g_textureCube             ) { g_shader->SetVar(g_textureCube             , grcTexture::None); }
	if (var != g_texture2DArray          ) { g_shader->SetVar(g_texture2DArray          , grcTexture::None); }
	if (var != g_textureCubeArray        ) { g_shader->SetVar(g_textureCubeArray        , grcTexture::None); }

	if (pixelCoordSpace == eDTQPCS_Pixels)
	{
		const float sx = 2.0f/(float)GRCDEVICE.GetWidth ();
		const float sy = 2.0f/(float)GRCDEVICE.GetHeight();

		x1 = +(sx*x1 - 1.0f);
		y1 = -(sy*y1 - 1.0f);
		x2 = +(sx*x2 - 1.0f);
		y2 = -(sy*y2 - 1.0f);
	}
	else if (pixelCoordSpace == eDTQPCS_NormalisedZeroToOne)
	{
		x1 = x1*2.0f - 1.0f;
		y1 = y1*2.0f - 1.0f;
		x2 = x2*2.0f - 1.0f;
		y2 = y2*2.0f - 1.0f;
	}
	else if (pixelCoordSpace == eDTQPCS_Normalised)
	{
		// no change
	}
	else
	{
		return;
	}

	DTQSetRenderState();
	AssertVerify(g_shader->BeginDraw(grmShader::RMC_DRAW, false, g_technique));

	g_shader->Bind(pass);
	{
		grcBegin(drawTriStrip, 4);
		grcVertex(x1, y1, z, 0.0f, 0.0f, -1.0f, colour, u1, v1);
		grcVertex(x1, y2, z, 0.0f, 0.0f, -1.0f, colour, u1, v2);
		grcVertex(x2, y1, z, 0.0f, 0.0f, -1.0f, colour, u2, v1);
		grcVertex(x2, y2, z, 0.0f, 0.0f, -1.0f, colour, u2, v2);
		grcEnd();
	}
	g_shader->UnBind();
	g_shader->EndDraw();

	if (var != grcevNONE)
	{
		g_shader->SetVar(var, grcTexture::None);
	}
}

} // namespace DTQ

#endif // __BANK
