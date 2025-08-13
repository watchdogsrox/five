#ifndef PRAGMA_DCL
	#pragma dcl position diffuse texcoord0 normal 
	#define PRAGMA_DCL
#endif
//
// albedo_alpha - albedo-only shader (no lighting) to match Sprite3D's in Wolf3D game;
//
// 2020/07/16	- Andrzej:	- initial;
//
//
#define ALPHA_SHADER
#include "../Megashader/megashader.fxh"


//
//
//
//
OutHalf4Color PS_AlbedoOnly_Unlit(vertexOutputUnlit IN)
{
	float4 ret = PixelTexturedUnlit(IN);
	return CastOutHalf4Color(half4(ret.rgb*gEmissiveScale, ret.a));
}

//
//
//
//
OutHalf4Color PS_AlbedoOnly(vertexOutput IN)
{
vertexOutputUnlit unlitIN = (vertexOutputUnlit)0;
	unlitIN.pos		= IN.pos;
	unlitIN.color0	= IN.color0;
	unlitIN.texCoord= IN.texCoord;

	float4 ret = PixelTexturedUnlit(unlitIN);
	return CastOutHalf4Color(half4(ret.rgb*gEmissiveScale, ret.a));
}




#if __MAX
technique tool_draw
{
	pass P0
	{
		MAX_TOOL_TECHNIQUE_RS
		VertexShader= compile VERTEXSHADER	VS_MaxTransform();
		PixelShader	= compile PIXELSHADER	PS_MaxPixelTextured();
	}
}
#else //__MAX...

technique draw
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER	VS_Transform(gbOffsetEnable);
		PixelShader  = compile PIXELSHADER	PS_AlbedoOnly()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
	}
}

technique drawskinned
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER	VS_TransformSkin(gbOffsetEnable);
		PixelShader  = compile PIXELSHADER	PS_AlbedoOnly()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
	}
}

technique lightweight0_draw
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER	VS_Transform(gbOffsetEnable);
		PixelShader  = compile PIXELSHADER	PS_AlbedoOnly()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
	}
}

technique lightweight0_drawskinned
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER	VS_TransformSkin(gbOffsetEnable);
		PixelShader  = compile PIXELSHADER	PS_AlbedoOnly()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
	}
}

technique lightweight4_draw
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER	VS_Transform(gbOffsetEnable);
		PixelShader  = compile PIXELSHADER	PS_AlbedoOnly()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
	}
}

technique lightweight4_drawskinned
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER	VS_TransformSkin(gbOffsetEnable);
		PixelShader  = compile PIXELSHADER	PS_AlbedoOnly()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
	}
}

technique lightweight8_draw
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER	VS_Transform(gbOffsetEnable);
		PixelShader  = compile PIXELSHADER	PS_AlbedoOnly()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
	}
}

technique lightweight8_drawskinned
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER	VS_TransformSkin(gbOffsetEnable);
		PixelShader  = compile PIXELSHADER	PS_AlbedoOnly()  CGC_FLAGS(CGC_FORWARD_LIGHTING_FLAGS);
	}
}

technique unlit_draw
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER	VS_TransformUnlit(gbOffsetEnable);
		PixelShader  = compile PIXELSHADER	PS_AlbedoOnly_Unlit()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}

technique unlit_drawskinned
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER	VS_TransformSkinUnlit(gbOffsetEnable);
		PixelShader  = compile PIXELSHADER	PS_AlbedoOnly_Unlit()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}


#if !defined(SHADER_FINAL)
#if USE_PN_TRIANGLES

#if __SHADERMODEL == 40
	technique entityselect_drawtessellated
	{
		pass p0
		{
	#include "megashader_deferredRS.fxh"
			VertexShader = compile VERTEXSHADER	VS_TransformD(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_EntitySelectID_BANK_ONLY()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
#endif //__SHADERMODEL == 40

technique SHADER_MODEL_50_OVERRIDE(entityselect_drawtessellated)
{
	pass p0
	{
		#include "megashader_deferredRS.fxh"
		VertexShader = compile VSDS_SHADER VS_northVertexInputBump_PNTri(gbOffsetEnable);
		SetHullShader(compileshader(hs_5_0, HS_northVertexInputBump_PNTri()));
		SetDomainShader(compileshader(ds_5_0, DS_TransformD_PNTri()));
		PixelShader = compile PIXELSHADER PS_EntitySelectID_BANK_ONLY()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}
#endif // USE_PN_TRIANGLES

#if USE_PN_TRIANGLES && DRAWSKINNED_TECHNIQUES
#if __SHADERMODEL == 40
	technique entityselect_drawskinnedtessellated
	{
		pass p0
		{
	#include "megashader_deferredRS.fxh"
			VertexShader = compile VERTEXSHADER	VS_TransformSkinD(gbOffsetEnable);
			PixelShader  = compile PIXELSHADER	PS_EntitySelectID_BANK_ONLY()  CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}
#endif //__SHADERMODEL == 40
technique SHADER_MODEL_50_OVERRIDE(entityselect_drawskinnedtessellated)
{
	pass p0
	{
		#include "megashader_deferredRS.fxh"
		VertexShader = compile VSDS_SHADER VS_northSkinVertexInputBump_PNTri(gbOffsetEnable);
		SetHullShader(compileshader(hs_5_0, HS_northVertexInputBump_PNTri()));
		SetDomainShader(compileshader(ds_5_0, DS_TransformD_PNTri()));
		PixelShader = compile PIXELSHADER PS_EntitySelectID_BANK_ONLY()  CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}
#endif // USE_PN_TRIANGLES
#endif // !__FINAL

#include "../Debug/debug_overlay_megashader.fxh"
#endif	//!__MAX...


