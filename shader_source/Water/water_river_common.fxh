#if __XENON || __WIN32PC || __PSSL
#define COMPUTE_FLOW_IN_VS 1
#else
#define COMPUTE_FLOW_IN_VS 0
#endif

BeginConstantBufferDX10(foam_globals);

//------------------------------------------------------------------------
// Foam Params
float FoamBumpiness : FoamBumpiness
<
	string UIName = "Foam_Bumpiness";
	string UIWidget = "Numeric";
	float UIMin = 0.00;
	float UIMax = 50.00;
	float UIStep = 1.0;
> = 5.0;

EndConstantBufferDX10(foam_globals);

BeginSampler	(sampler, FlowTexture,FlowSampler, FlowTexture)
	string	UIName				= "Flow Map";
	string	UvSetName			= "FogAndFoamColorMap";
	int		UvSetIndex			= 1;
	string 	TCPTemplateRelative	= "maps_other/WaterFlowMap";
	int		TCPAllowOverride	= 0;
	string	TextureType			= "Flow";
#ifdef IS_SHALLOW
	int		nostrip				= 1;//required for physics flow sampling from shallow shaders
#endif //IS_SHALLOW
ContinueSampler	(sampler, FlowTexture, FlowSampler, FlowTexture)
		AddressU	= CLAMP;
		AddressV	= CLAMP;
		MIPFILTER	= MIPLINEAR;
		MINFILTER	= HQLINEAR;
		MAGFILTER	= MAGLINEAR;
EndSampler;

BeginSampler	(sampler, FogTexture, FogSampler, FogTexture)
	string UIName				= "Fog Map";
	string	UvSetName			= "FogMap";
	int		UvSetIndex			= 1;
	string 	TCPTemplateRelative	= "maps_other/WaterFogFoamMap";
	int		TCPAllowOverride	= 0;
	string	TextureType			= "Fog";
ContinueSampler	(sampler, FogTexture, FogSampler, FogTexture)
		AddressU	= CLAMP;
		AddressV	= CLAMP;
		MIPFILTER	= LINEAR;
		MINFILTER	= LINEAR;
		MAGFILTER	= LINEAR;
EndSampler;
