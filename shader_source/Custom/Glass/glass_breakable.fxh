
#include "../../Megashader/megashader.fxh"

//------------------------------------------------------------------------------
// Code placed textures
//------------------------------------------------------------------------------

BeginSampler(sampler2D,decalTexture,DecalSampler,DecalTex )
ContinueSampler(sampler2D,decalTexture,DecalSampler,DecalTex )
	AddressU  = BORDER;
	AddressV  = BORDER;
	ANISOTROPIC_LEVEL(4)
	MIPFILTER = MIPLINEAR;
	MINFILTER = MINANISOTROPIC;
	MAGFILTER = MAGLINEAR;
	MipMapLodBias = 0;
	BorderColor = 0x00000000; 
EndSampler;

BeginSampler(sampler2D,brokenBumpTexture,BrokenBumpSampler,BrokenBumpTex)
ContinueSampler(sampler2D,brokenBumpTexture,BrokenBumpSampler,BrokenBumpTex)
	AddressU  = WRAP;        
	AddressV  = WRAP;
	AddressW  = WRAP;
	ANISOTROPIC_LEVEL(4)
	MIPFILTER = MIPLINEAR;
	MINFILTER = MINANISOTROPIC;
	MAGFILTER = MAGLINEAR;
EndSampler;

BEGIN_RAGE_CONSTANT_BUFFER(glass_breakable_locals,b9)
float4 BrokenDiffuseColor
<
	string UIName = "BrokenDiffuseColor";
	float UIMin = 0.0;
	float UIMax = 1.0;
	float UIStep = 0.01;
	int nostrip	= 1;	// don't strip
> = float4(117.3/255.0,156.0/255.0,156.0/255.0, 145.0/255.0);

float4 BrokenSpecularColor
<
	string UIName = "BrokenSpecularColor";
	float UIMin = 0.0;
	float UIMax = 1.0;
	float UIStep = 0.01;
	int nostrip	= 1;	// don't strip
> = float4(117.3/255.0,156.0/255.0,156.0/255.0, 1.0);

// Ideally, I'd want a float3x2 here, or even a float2x2, but the shader compiler doesn't seem to support those types.
// Transform from base texture coords to crack mask texture coords, rotating and scaling as necessary
float4 CrackMatrix = float4(1.f, 0.f, 0.0f, 1.0f);
// 'w' value of 0 indicates no breakage yet
float4 CrackOffset = float4(0.f, 0.f, 0.0f, 0.0f);

float2 CrackEdgeBumpTileScale : CrackEdgeBumpTileScale
<
string UIName = "Crack Edge Bump Tile Scale";
string UIWidget = "Numeric";
float UIMin = 0.00;
float UIMax = 100.00;
float UIStep = 0.001;
int nostrip	= 1;	// don't strip
> = float2(1.0, 1.0);

float2 CrackDecalBumpTileScale : CrackDecalBumpTileScale
<
string UIName = "Crack Decal Bump Tile Scale";
string UIWidget = "Numeric";
float UIMin = 0.00;
float UIMax = 100.00;
float UIStep = 0.001;
int nostrip	= 1;	// don't strip
> = float2(1.0, 1.0);

float CrackEdgeBumpAmount : CrackEdgeBumpAmount
<
string UIName = "Crack Edge Bump Amount";
string UIWidget = "Numeric";
float UIMin = 0.00;
float UIMax = 100.00;
float UIStep = 0.01;
int nostrip	= 1;	// don't strip
> = 0.0f;

float CrackDecalBumpAmount : CrackDecalBumpAmount
<
string UIName = "Crack Decal Bump Amount";
string UIWidget = "Numeric";
float UIMin = 0.00;
float UIMax = 100.00;
float UIStep = 0.01;
int nostrip	= 1;	// don't strip
> = 0.0f;

float CrackDecalBumpAlphaThreshold : CrackDecalBumpAlphaThreshold
<
string UIName = "Crack Decal Bump Alpha Threshold";
string UIWidget = "Numeric";
float UIMin = 0.00;
float UIMax = 1.00;
float UIStep = 0.001;
int nostrip	= 1;	// don't strip
> = 0.0f;

float4 DecalTint
<
	string UIName = "DecalTint";
	float UIMin = 0.0;
	float UIMax = 1.0;
	float UIStep = 0.01;
	int nostrip	= 1;	// don't strip
> = float4(1.0, 1.0, 1.0, 1.0);

float4 gDecalChannelSelection[4] = {{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}};

EndConstantBufferDX10( glass_breakable_locals )

//------------------------------------------------------------------------------
// Vertex shader
//------------------------------------------------------------------------------
struct bgVertexInput
{
    float3 pos : POSITION;
	float4 diffuse : COLOR0;
    float2 texCoord0 : TEXCOORD0;
    float2 texCoord1 : TEXCOORD1;
    float3 normal : NORMAL;
	float4 tangent : TANGENT;
};

struct bgVertexInputLow
{
    float3 pos : POSITION;
	float4 diffuse : COLOR0;
    float2 texCoord0 : TEXCOORD0;
    float3 normal : NORMAL;
	float4 tangent : TANGENT;
};

struct bgVertexInputCrack
{
    float3 pos : POSITION;
	float4 diffuse : COLOR0;
    float2 texCoord0 : TEXCOORD0;
    float2 texCoord1 : TEXCOORD1;
    float3 normal : NORMAL;
};

struct bgVertexOutput
{
	float4 decalTint : COLOR0;
	float4 decalMask : COLOR1;
	float4 texCoord : TEXCOORD0;
	float3 worldNormal : TEXCOORD1;
	float3 worldTangent : TEXCOORD2;
	float3 worldBinormal : TEXCOORD3;
	float3 worldEyePos : TEXCOORD4;
	float3 worldPos : TEXCOORD5;
	float2 alphaCrackValue : TEXCOORD6;
    DECLARE_POSITION_CLIPPLANES(pos)
};

struct bgVertexOutputLow
{
	float4 decalTint : COLOR0;
	float4 decalMask : COLOR1;
	float4 texCoord : TEXCOORD0;
	float3 worldPos : TEXCOORD1;
	float3 worldNorm : TEXCOORD2;
	float4 lightDiffuse : TEXCOORD3; // Diffuse light + Kd
	float4 lightSpecular : TEXCOORD4; // Specular light + specular exponent
	
#if VS_LIT_GLASS_WITH_PS_SHADOWS
	float4 localLightDiffuse : TEXCOORD5; // Diffuse local lights
	float4 localLightSpecular : TEXCOORD6; // Specular local lights
#endif // VS_LIT_GLASS_WITH_PS_SHADOWS

	// These two store the back light values
	// We seperate them because we need the final opacity value to combine them with the front light (sampled from a texture on the pixel shader)
	float4 localLightDiffuse2 : TEXCOORD7; // Diffuse light + alphaX
	float4 localLightSpecular2 : TEXCOORD8; // Specular light + specular exponent

	float3 lightAmbientEmissive : TEXCOORD9; // Ambient + emissive lights
    DECLARE_POSITION_CLIPPLANES(pos)
};

bgVertexOutput VSBreakableGlass(bgVertexInput IN, bool renderCrack, bool renderCrackEdge)
{
	bgVertexOutput OUT;

	//When a piece is displaced because of an impact, rotation/translation get applied here
	//The diffuse w is the actual index
    float3 pos = IN.pos.xyz;
    float3 normal = IN.normal;
    float alphaValue = 1.0;
	float4 tangent = IN.tangent;
	float4 actualDecalTint = 0;
	float4 decalMask = 0;

	float surface_specularIntensity = 0;
    
	if( renderCrack )
	{
		rageSkinMtx transform = gBoneMtx[uint(round(IN.diffuse.w*255))];

		// Get scale from matrix and store as fade value
		float fadeValue = length(transform[0].xyz);
		if (0 < fadeValue)
		{
			// Remove scale from matrix
			transform[0].xyz /= fadeValue;
			transform[1].xyz /= fadeValue;
			transform[2].xyz /= fadeValue;
		}
		pos = rageSkinTransform(pos, transform);
		normal = IN.diffuse.y ? rageSkinRotate(normal, transform) : normal;
		tangent.xyz = transform[0].xyz; // Only the pane uses the tangent so no need to lerp
		
		alphaValue = lerp(fadeValue, IN.diffuse.x * fadeValue, IN.diffuse.y);
		if(!renderCrackEdge)
		{
			actualDecalTint = DecalTint;
		}

		surface_specularIntensity = dot(BrokenSpecularColor.xyz, specMapIntMask);
		
		decalMask = gDecalChannelSelection[uint(round(IN.diffuse.z*255))]; // Select the decal channel mask
	}

	float3 worldPos = mul(float4(pos,1), (float4x3)gWorld);

	OUT.pos = mul(float4(pos,1), gWorldViewProj);

	OUT.worldEyePos = gViewInverse[3].xyz - worldPos;
	
	OUT.texCoord = float4(IN.texCoord0,IN.texCoord1);
	
	OUT.worldNormal.xyz	= normalize(mul(normal, (float3x3)gWorld));
	OUT.worldNormal.xyz *= sign(dot(OUT.worldNormal.xyz,OUT.worldEyePos.xyz));
	
	OUT.worldTangent	= normalize(mul(tangent, (float3x3)gWorld)).xyz;
	OUT.worldBinormal	= rageComputeBinormal(OUT.worldNormal, OUT.worldTangent, tangent.w);

	OUT.decalTint = actualDecalTint;
	OUT.worldPos = worldPos;
	
	OUT.alphaCrackValue.x = alphaValue;
#ifdef VS_LIT_WITH_VERT_ALPHA
	OUT.alphaCrackValue.x *= IN.diffuse.w;
#endif // VS_LIT_WITH_VERT_ALPHA

	OUT.alphaCrackValue.y = surface_specularIntensity;
	
	OUT.decalMask = decalMask;
		
	RAGE_COMPUTE_CLIP_DISTANCES(OUT.pos);

	return OUT;
}

#if USE_SHADOW_RECEIVING_VS || VS_LIT_GLASS_WITH_PS_SHADOWS
bgVertexOutputLow VSBreakableGlass_Low(bgVertexInput IN, int numLights, bool renderCrack, bool renderCrackEdge)
{
	bgVertexOutputLow OUT;
	bgVertexInput vtxIn;
	vtxIn.pos = IN.pos;
	vtxIn.diffuse = IN.diffuse;
	vtxIn.texCoord0 = IN.texCoord0;
	vtxIn.texCoord1 = IN.texCoord1;
	vtxIn.normal = IN.normal;
	vtxIn.tangent = IN.tangent;

	bgVertexOutput OutBase = VSBreakableGlass(vtxIn,renderCrack, renderCrackEdge);
	
	// Copy stuff from the base struct
	OUT.pos = OutBase.pos;	
	OUT.texCoord = OutBase.texCoord;
	OUT.worldPos = OutBase.worldPos;
	OUT.worldNorm = OutBase.worldNormal;
	OUT.localLightDiffuse2.w = OutBase.alphaCrackValue.x;
	OUT.decalTint = OutBase.decalTint;
	OUT.decalMask = OutBase.decalMask;

	// Get the surface info without the diffuse texture

#if PV_AMBIENT_BAKES
	IN.diffuse.g *= ArtificialBakeAdjustMult();
	SurfaceProperties surfaceInfo = GetSurfacePropertiesVeryBasic(IN.diffuse, OutBase.worldNormal.xyz);
#else
	SurfaceProperties surfaceInfo = GetSurfacePropertiesVeryBasic(float4(1.0, ArtificialBakeAdjustMult(), 1.0, OutBase.alphaCrackValue.x), OutBase.worldNormal.xyz);
#endif

	// This is kind of ugly but for some reason GetSurfacePropertiesBasic doesn't take this into account
	#if SPECULAR
		surfaceInfo.surface_specularIntensity = specularIntensityMult;
		surfaceInfo.surface_specularExponent = specularFalloffMult;	
		surfaceInfo.surface_fresnel = specularFresnel;
	#endif

	StandardLightingProperties surfaceStandardLightingProperties = DeriveLightingPropertiesForCommonSurface( surfaceInfo );	
	surfaceStandardLightingProperties.diffuseColor = ProcessDiffuseColor(surfaceStandardLightingProperties.diffuseColor);
	
	surfaceProperties surface;
	directionalLightProperties light;
	materialProperties material;
	
	populateForwardLightingStructs(
		surface,
		material,
		light,
		OutBase.worldPos.xyz,
		surfaceInfo.surface_worldNormal.xyz,
		surfaceStandardLightingProperties);
	
	LightingResult res = directionalCalculateLighting(
		surface, // surfaceProperties surface
		material,
		light,
		true, // specular
		true, // reflection
		true, // directional
			false,	// receiveShadows
				false, // receiveShadowsFast
				false, // receiveshadowsHighQuality
			float2(0.0, 0.0), // screenPos
			true,	// directionalDiffuse
			true,	// directionalSpecular
		false,		// useBackLighting
		false,		// scatter
		false);		// useCloudShadows

#if USE_SHADOW_RECEIVING_VS
	// Take shadows into account when calculating the final attenuation
	res.lightAttenuation *= CalcCascadeShadowSampleVS(OUT.worldPos.xyz, true, true);
#endif // USE_SHADOW_RECEIVING_VS

#if SELF_SHADOW
	res.lightAttenuation *= material.selfShadow;
#endif // SELF_SHADOW

	Components components;
	GetLightValues(res, 
		true, // diffuse
		true, // specular,
		true, // reflection
		components,
		OUT.lightDiffuse.rgb,
		OUT.lightSpecular.rgb
		);

	OUT.localLightSpecular2.w = res.material.diffuseColor.a;
	
	// Take attenuation into account
	OUT.lightDiffuse.rgb *= res.lightColor * res.lightAttenuation;
	OUT.lightSpecular.rgb *= res.lightColor * res.lightAttenuation;
	
	// Add the local lights
	float3 localLightDiffuse;
	#if SPECULAR
		float3 localLightSpecular;
	#endif // SPECULAR

	calculateLocalLightContributionInternal(
		numLights, 
		true, 
		surface, 
		material, 
		components, 
		true, 
		1.0f,
		localLightDiffuse
		#if SPECULAR
		, true
		, 1.0
		, res.surface.eyeDir
		, localLightSpecular
		#endif // SPECULAR
		);

	OUT.lightDiffuse.a = 1.0f - components.Kd; // Keep this around for reflection calculation

#if VS_LIT_GLASS_WITH_PS_SHADOWS
	OUT.localLightDiffuse.rgb = localLightDiffuse * components.Kd;
#if SPECULAR
	OUT.localLightSpecular.rgb = localLightSpecular * components.Ks;
#endif // SPECULAR

#else
	OUT.lightDiffuse.rgb += localLightDiffuse * components.Kd;
#if SPECULAR
	OUT.lightSpecular.rgb += localLightSpecular * components.Ks;
#endif // SPECULAR
#endif // VS_LIT_GLASS_WITH_PS_SHADOWS

	// Do the back-lights calculation by flipping the normal
	surface.normal = -surface.normal;
	calculateLocalLightContributionInternal(
		numLights, 
		true, 
		surface, 
		material, 
		components, 
		true, 
		1.0f,
		localLightDiffuse
		#if SPECULAR
		, true
		, 1.0
		, res.surface.eyeDir
		, localLightSpecular
		#endif // SPECULAR
		);
		
	OUT.localLightDiffuse2.xyz = localLightDiffuse * components.Kd;
#if SPECULAR
	OUT.localLightSpecular2.xyz = localLightSpecular * components.Ks;
#endif // SPECULAR

	// Restore the normal
	surface.normal = -surface.normal;

	OUT.lightSpecular.a = material.specularExponent;

	// Ambient contribution
#if !defined(NO_FORWARD_AMBIENT_LIGHT)
		OUT.lightAmbientEmissive = calculateAmbient(
			true, 
			true, 
			true, 
			true, 
			surface, 
			material,
			components.Kd); 
#endif

#if EMISSIVE
		OUT.lightAmbientEmissive += material.emissiveIntensity;
#endif // EMISSIVE

#if VS_LIT_GLASS_WITH_PS_SHADOWS
	OUT.localLightDiffuse.a = material.naturalAmbientScale;
	OUT.localLightSpecular.a = material.artificialAmbientScale; 
#endif

	RAGE_COMPUTE_CLIP_DISTANCES(OUT.pos);

	return OUT;
}
#endif // USE_SHADOW_RECEIVING_VS || VS_LIT_GLASS_WITH_PS_SHADOWS

//------------------------------------------------------------------------------
// BumpBrokenNormal
//------------------------------------------------------------------------------
float3 BumpBrokenNormal(float3 normal, float2 noiseTexCoords, bool renderCrackEdge)
{
	float2 vBrokenTex = (renderCrackEdge?CrackEdgeBumpTileScale.xy:CrackDecalBumpTileScale.xy) * noiseTexCoords;
	float3 vOffset = (tex2D(BrokenBumpSampler,vBrokenTex)*2-1).xyz;
	//Not using any tangent space transformations as we're applying a noisy bump texture and
	//it's cheaper to just apply the bump to the input normal without any transformations
	normal.xy += vOffset.xy * (renderCrackEdge?CrackEdgeBumpAmount:CrackDecalBumpAmount);
	normal = normalize(normal);		
	return normal;

}
//------------------------------------------------------------------------------
// Main pixel shader code
//------------------------------------------------------------------------------
half4 breakableGlassRender( bgVertexOutput IN, int numLights, bool directionalLightAndShadow, bool bUseCapsule, bool bUseHighQuality, bool renderCrack, bool renderCrackEdge, bool renderCrackBumps)
{
	float3 normal;
	normal = (IN.worldNormal.xyz); //There is a normalize already in the GetSurfaceProperties function. 

	float4 cDecal;
	float4 decalTint;
	if(renderCrack)
	{
		cDecal = dot(IN.decalMask, tex2D(DecalSampler,IN.texCoord.zw)).xxxx;
		decalTint = cDecal * IN.decalTint;
		bool bApplyBrokenNormal = renderCrackBumps && (renderCrackEdge || (decalTint.w > CrackDecalBumpAlphaThreshold));
		if(bApplyBrokenNormal)
		{
			normal = BumpBrokenNormal(normal, IN.texCoord.xy, renderCrackEdge);
		}
	}
	
	// BREAKABLE
	SurfaceProperties surfaceInfo = GetSurfaceProperties(
			float4(1,1,1,1),
#if DIFFUSE_TEXTURE
			IN.texCoord.xy,
			DiffuseSampler,
#endif // DIFFUSE_TEXTURE
#if SPEC_MAP
			IN.texCoord.xy,
	#if SPECULAR_DIFFUSE
			DiffuseSampler,
	#else
			SpecSampler,
	#endif // SPECULAR_DIFFUSE
#endif	// SPEC_MAP
			IN.worldEyePos,
			ReflectionSampler,
#if NORMAL_MAP
			IN.texCoord.xy,
			BumpSampler, 
			IN.worldTangent,
			IN.worldBinormal,
#endif // NORMAL_MAP
			normal
		);

	if( renderCrack )
	{
		// Get base color for broken and whole pane of glass
		float4 cWhole = surfaceInfo.surface_diffuseColor;
		float4 cBroken = BrokenDiffuseColor;
		float4 cDiffuse = saturate((renderCrackEdge?cBroken:cWhole) + decalTint);

		float4 cSpecBroken= BrokenSpecularColor;
		float2 surfaceInfo_specularExponent_specularIntensity = float2(surfaceInfo.surface_specularExponent, surfaceInfo.surface_specularIntensity);
		float2 surface_specularExponent_specularIntensity = float2(cSpecBroken.w, saturate(IN.alphaCrackValue.y));

		surface_specularExponent_specularIntensity = (renderCrackEdge?surface_specularExponent_specularIntensity:surfaceInfo_specularExponent_specularIntensity);				
		surface_specularExponent_specularIntensity = max(surface_specularExponent_specularIntensity, float2(cDecal.w, dot(cDecal.xyz, specMapIntMask)));	

		surfaceInfo.surface_diffuseColor = cDiffuse;
		surfaceInfo.surface_specularExponent = surface_specularExponent_specularIntensity.x;
		surfaceInfo.surface_specularIntensity = surface_specularExponent_specularIntensity.y;
	}
	
	StandardLightingProperties surfaceStandardLightingProperties = DeriveLightingPropertiesForCommonSurface( surfaceInfo );
	surfaceStandardLightingProperties.diffuseColor = ProcessDiffuseColor(surfaceStandardLightingProperties.diffuseColor);

	float4 OUT;

	surfaceProperties surface;
	directionalLightProperties light;
	materialProperties material;

	populateForwardLightingStructs(
		surface,
		material,
		light,
		IN.worldPos.xyz,
		surfaceInfo.surface_worldNormal.xyz,
		surfaceStandardLightingProperties);

#if __XENON
	[isolate]
#endif
	OUT = calculateForwardLighting(
		numLights,
		bUseCapsule,
		surface,
		material,
		light,
		directionalLightAndShadow,
			SHADOW_RECEIVING,
			bUseHighQuality,
		IN.pos.xy * gooScreenSize.xy);

#if defined(USE_FOGRAY_FORWARDPASS) && !__LOW_QUALITY
	if (gUseFogRay)
		OUT =  calcDepthFxBasic(OUT, IN.worldPos.xyz, IN.pos.xy * gooScreenSize.xy);
	else
		OUT = calcDepthFxBasic(OUT, IN.worldPos.xyz);
#else
	OUT = calcDepthFxBasic(OUT, IN.worldPos.xyz);
#endif

	OUT.a *= IN.alphaCrackValue.x; // Final alpha value

#if TEST_IS_CRACK
	OUT.a = 1;
	
	if( renderCrack )
	{
		OUT.rgb *= float3(1,0,0);
	}
	else
	{
		OUT.rgb *= float3(0,1,0);
	}
#endif

	return PackHdr(OUT);
}

half4 breakableGlassRenderLow(bgVertexOutputLow IN, bool renderCrack)
{
	// Get the basic window diffuse color
#if DIFFUSE_TEXTURE
	float4 matDiffuse = tex2D(DiffuseSampler, IN.texCoord.xy);
#else
	float4 matDiffuse = float4(1.0, 1.0, 1.0, 1.0);
#endif // DIFFUSE_TEXTURE
	
	matDiffuse.a *= IN.localLightSpecular2.w;
	if( renderCrack )
	{
		matDiffuse.a *= IN.localLightDiffuse2.w;
	}

	matDiffuse = calculateDiffuseColor(matDiffuse);
	
	// Change the diffuse color for cracks
	if( renderCrack )
	{
		// Get base color for broken and whole pane of glass
		float4 cWhole = matDiffuse;
		float4 cBroken = BrokenDiffuseColor;
		float4 cDecal = dot(IN.decalMask, tex2D(DecalSampler,IN.texCoord.zw)).xxxx;
		float4 cDiffuse = saturate(cWhole + cDecal * IN.decalTint);
		matDiffuse = cDiffuse;
	}
	
	matDiffuse = ProcessDiffuseColor(matDiffuse);
	
	float3 lightDiffuse = IN.lightDiffuse.rgb;
	float3 lightSpecular = IN.lightSpecular.rgb;
#if VS_LIT_GLASS_WITH_PS_SHADOWS
	// Apply the cascaded shadows
	float shadowAmount = CalcCascadeShadowsFastNoFade(gViewInverse[3].xyz, IN.worldPos, IN.worldNorm, IN.pos.xy);
	lightDiffuse *= shadowAmount;
	lightSpecular *= shadowAmount;
	
	// Add the local lights
	lightDiffuse += IN.localLightDiffuse.rgb;
	lightSpecular += IN.localLightSpecular.rgb;
#endif // VS_LIT_GLASS_WITH_PS_SHADOWS

	// Add the back lights
	lightDiffuse += IN.localLightDiffuse2.xyz * matDiffuse.a;
	lightSpecular += IN.localLightSpecular2.xyz * matDiffuse.a;
	
	half4 OUT = calculateForwardLightingSimple(lightDiffuse, lightSpecular, IN.lightAmbientEmissive, matDiffuse);
	
	// Reflection contribution
	const float3 surfaceToEye = gViewInverse[3].xyz - IN.worldPos.xyz;
	const float3 surfaceToEyeDir = normalize(surfaceToEye);

	// No need to set material.reflectionColor - it will be sampled from the env map inside calculateReflection
	materialProperties material = (materialProperties)0;
	material.specularExponent = IN.lightSpecular.a;
	material.reflectionIntensity = 1.0;
#if VS_LIT_GLASS_WITH_PS_SHADOWS
	material.naturalAmbientScale = IN.localLightDiffuse.a;
	material.artificialAmbientScale = IN.localLightSpecular.a;
#else
	material.naturalAmbientScale = gNaturalAmbientScale;
	material.artificialAmbientScale = gArtificialAmbientScale * ArtificialBakeAdjustMult();
#endif

	OUT.rgb += calculateReflection(
		IN.worldNorm, 
		material, 
		surfaceToEyeDir, 
		IN.lightDiffuse.a,
		false); // No back lighting 
	
#if defined(USE_FOGRAY_FORWARDPASS) && !__LOW_QUALITY
	if (gUseFogRay)
		OUT =  calcDepthFxBasic(OUT, IN.worldPos.xyz, IN.pos.xy * gooScreenSize.xy);
	else
		OUT = calcDepthFxBasic(OUT, IN.worldPos.xyz);
#else
	OUT = calcDepthFxBasic(OUT, IN.worldPos.xyz);
#endif

#if TEST_IS_CRACK
	OUT.a = 1;
	
	if( renderCrack )
	{
		OUT.rgb *= float3(1,0,0);
	}
	else
	{
		OUT.rgb *= float3(0,1,0);
	}
#endif

	return PackHdr(OUT);
}

//------------------------------------------------------------------------------
// Deferred pixel shader code
//------------------------------------------------------------------------------
DeferredGBuffer breakableGlassRenderDeferred( bgVertexOutput IN,bool renderCrack, bool renderCrackEdge, bool renderCrackBumps)
{
	float3 normal;
	normal = IN.worldNormal.xyz; //There is a normalize already in the GetSurfaceProperties function. 

	float4 cDecal;
	float4 decalTint;
	if(renderCrack)
	{
		float2 vWholeTex = IN.texCoord.xy;
		cDecal = dot(IN.decalMask, tex2D(DecalSampler,IN.texCoord.zw)).xxxx;
		decalTint = cDecal * IN.decalTint;
		bool bApplyBrokenNormal = renderCrackBumps && (renderCrackEdge || (decalTint.w > CrackDecalBumpAlphaThreshold));
		if(bApplyBrokenNormal)
		{
			normal = BumpBrokenNormal(normal, IN.texCoord.xy, renderCrackEdge);
		}
	}
	// BREAKABLE
	SurfaceProperties surfaceInfo = GetSurfaceProperties(
			float4(1,1,1,1),
#if DIFFUSE_TEXTURE
			IN.texCoord.xy,
			DiffuseSampler,
#endif
#if SPEC_MAP
			IN.texCoord.xy,
	#if SPECULAR_DIFFUSE
			DiffuseSampler,
	#else
			SpecSampler,
	#endif // SPECULAR_DIFFUSE
#endif	// SPEC_MAP
			IN.worldEyePos,
			ReflectionSampler,
#if NORMAL_MAP
			IN.texCoord.xy,
			BumpSampler, 
			IN.worldTangent,
			IN.worldBinormal,
#endif // NORMAL_MAP
			normal
		);

	if( renderCrack )
	{
		// Get base color for broken and whole pane of glass
		float4 cWhole = surfaceInfo.surface_diffuseColor;
		float4 cBroken = BrokenDiffuseColor;
		float4 cDiffuse = saturate((renderCrackEdge?cBroken:cWhole) + decalTint);

		float4 cSpecBroken= BrokenSpecularColor;
		float2 surfaceInfo_specularExponent_specularIntensity = float2(surfaceInfo.surface_specularExponent, surfaceInfo.surface_specularIntensity);
		float2 surface_specularExponent_specularIntensity = float2(cSpecBroken.w, IN.alphaCrackValue.y);


		surface_specularExponent_specularIntensity = (renderCrackEdge?surface_specularExponent_specularIntensity:surfaceInfo_specularExponent_specularIntensity);
		surface_specularExponent_specularIntensity = max(surface_specularExponent_specularIntensity, float2(cDecal.w, dot(cDecal.xyz, specMapIntMask)));

		surfaceInfo.surface_diffuseColor = cDiffuse;
		surfaceInfo.surface_specularExponent = surface_specularExponent_specularIntensity.x;
		surfaceInfo.surface_specularIntensity = surface_specularExponent_specularIntensity.y;
	}
	
	StandardLightingProperties surfaceStandardLightingProperties = DeriveLightingPropertiesForCommonSurface( surfaceInfo );

	DeferredGBuffer OUT;

	if( renderCrack )
	{
		surfaceStandardLightingProperties.diffuseColor.a *= IN.alphaCrackValue.x;
	}
	
	// Pack the information into the GBuffer(s).
	OUT = PackGBuffer(	surfaceInfo.surface_worldNormal.xyz,
						surfaceStandardLightingProperties );

	return OUT;
}
