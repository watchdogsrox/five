// 
// clouds_anim.fx
// 
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved. 
//

#ifndef PRAGMA_DCL
	#pragma dcl position diffuse normal texcoord0 tangent0 
	#define PRAGMA_DCL
#endif

//No reflection for fog!
#define CLOUDS_NO_REFLECTION_TECHNIQUE
#define USE_FOG

#include "Clouds.fxh"


#if !__MAX
#if FORWARD_TECHNIQUES
technique draw
{
    pass p0
    {
        VertexShader = compile VERTEXSHADER VSCloudsFog();
		PixelShader = compile PIXELSHADER PSCloudFog() CGC_FLAGS(CGC_DEFAULTFLAGS);
    }
}
#endif // FORWARD_TECHNIQUES

//#if UNLIT_TECHNIQUES
technique unlit_draw
{
    pass p0
    {
        VertexShader = compile VERTEXSHADER VSCloudsFog();
		PixelShader = compile PIXELSHADER PSCloudFog() CGC_FLAGS(CGC_DEFAULTFLAGS);
    }
}
//#endif // UNLIT_TECHNIQUES

technique cloudlightning_draw
{
    pass p0
    {
        VertexShader = compile VERTEXSHADER VSCloudsFog();
		PixelShader = compile PIXELSHADER PSCloudFog() CGC_FLAGS(CGC_DEFAULTFLAGS);
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
		PixelShader = compile PIXELSHADER PSCloudsOccluder() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}

// same as normal draw technique for now, but without the depth lookup and soft particle alpha (for shaders that have that)
// it might get further simplifications or optimizations at a later time
technique waterreflection_draw
{
	pass p0
	{
		VertexShader = compile VERTEXSHADER VSCloudsFog();
		PixelShader = compile PIXELSHADER PSCloudFogWater() CGC_FLAGS(CGC_DEFAULTFLAGS);
	}
}
 #endif //!__MAX
