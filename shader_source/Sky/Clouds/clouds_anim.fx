// 
// clouds_anim.fx
// 
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved. 
//

#ifndef PRAGMA_DCL
	#pragma dcl position diffuse normal texcoord0 tangent0 
	#define PRAGMA_DCL
#endif

#define USE_FOG
#define MAX_USES_CLOUD_ANIM_TECH
#include "Clouds.fxh"


#if !__MAX
#if FORWARD_TECHNIQUES
technique draw
{
    pass p0
    {
		SETUP_CLOUD_STENCIL
		VertexShader = compile VERTEXSHADER VSCloudsVertScatterPiercing_Infinite();
		// Half precision is overflowing on the distance calculation. This probably
		// needs further investigation to see if we can turn it back on.
		PixelShader = compile PIXELSHADER PSCloudsVertScatterPiercingLightning_Anim() CGC_FLAGS("-texformat d RGBA8 -po OutColorPrec=fp16");
		//PixelShader = compile PIXELSHADER PSCloudsVertScatterPiercingLightning_Anim() CGC_FLAGS(CGC_DEFAULTFLAGS);
    }
}
#endif // FORWARD_TECHNIQUES

//#if UNLIT_TECHNIQUES
technique unlit_draw
{
    pass p0
    {
		SETUP_CLOUD_STENCIL
        VertexShader = compile VERTEXSHADER VSCloudsVertScatterPiercing_Infinite();
		PixelShader = compile PIXELSHADER PSCloudsVertScatterPiercing_Anim() CGC_FLAGS(CGC_DEFAULTFLAGS);
    }
}
//#endif // UNLIT_TECHNIQUES

technique cloudlightning_draw
{
    pass p0
    {
		SETUP_CLOUD_STENCIL
        VertexShader = compile VERTEXSHADER VSCloudsVertScatterPiercing_Infinite();
		// Half precision is overflowing on the distance calculation. This probably
		// needs further investigation to see if we can turn it back on.
		PixelShader = compile PIXELSHADER PSCloudsVertScatterPiercingLightning_Anim() CGC_FLAGS("-texformat d RGBA8 -po OutColorPrec=fp16");
    }
}

technique clouddepth_draw
{
	pass	p0
	{
		AlphaBlendEnable=true;
		srcBlend=SrcAlpha;
		destBlend=InvSrcAlpha;

		VertexShader = compile VERTEXSHADER VSCloudsOccluder();
		PixelShader = compile PIXELSHADER PSCloudsOccluder_Anim() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}

#if CLOUDS_IN_PARABOLOID_ENVMAP

	technique envmapparab_draw
	{
		pass	p0
		{
			AlphaTestEnable=false;
			AlphaBlendEnable=true;
			srcBlend=SrcAlpha;
			destBlend=InvSrcAlpha;
	#if __PS3
			BlendOp = Add;
			ZwriteEnable = false;
	#endif
			CullMode = NONE;
			ZEnable = false;

			VertexShader = compile VERTEXSHADER VSCloudsEnvMapParab();
			PixelShader = compile PIXELSHADER PSCloudsEnvMapParab_LowRes() CGC_FLAGS(CGC_DEFAULTFLAGS);
		}

		pass	p1
		{
			AlphaTestEnable=false;
			AlphaBlendEnable=true;
			srcBlend=SrcAlpha;
			destBlend=InvSrcAlpha;
	#if __PS3
			BlendOp = Add;
			ZwriteEnable = false;
	#endif
			CullMode = NONE;
			ZEnable = false;

			VertexShader = compile VERTEXSHADER VSCloudsEnvMapParab();
			PixelShader = compile PIXELSHADER PSCloudsEnvMapParab_LowRes() CGC_FLAGS(CGC_DEFAULTFLAGS);
		}
	}

#endif // #if CLOUDS_IN_PARABOLOID_ENVMAP

// same as normal draw technique for now, but without the depth lookup and soft particle alpha (for shaders that have that)
// it might get further simplifications or optimizations at a later time
technique waterreflection_draw
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER VSCloudsVertScatterPiercing_Infinite();
		PixelShader = compile PIXELSHADER PSCloudsVertScatterPiercing_AnimWater() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}

#endif //!__MAX
