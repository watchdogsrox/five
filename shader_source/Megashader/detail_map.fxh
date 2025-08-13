#ifndef DETAIL_MAP_FXH
#define DETAIL_MAP_FXH

BeginConstantBufferDX10(detail_map_locals)

#ifdef DETAIL_USE_VOLUME_TEXTURE

	#define USE_2D_ATLAS						(__MAX)
	#define USE_SPEC_BLUE_INDEX_MAP				1



	float3 detailSettings : detailSettings
	<
		string UIWidget = "Numeric";
		int nostrip=1;
		float UIMin = 0.0;
		float UIMax = 100.0;
		float UIStep = .001;
		string UIName = "detail Inten Bump Scale";
	> = {0.,0.,24.};
#else
	float4 detailSettings : detailSettings
	<
		string UIWidget = "Numeric";
		int nostrip=1;
		float UIMin = 0.0;
		float UIMax = 100.0;
		float UIStep = .001;
		string UIName = "detail Inten Bump Scale";
	> = {0.,0.,24.,24.};
#endif

EndConstantBufferDX10(detail_map_locals)

#ifdef DETAIL_USE_VOLUME_TEXTURE

	#define detailIntensity			(detailSettings.x)
	#define detailBumpIntensity		(detailSettings.y)
	#define detailScale				(detailSettings.z)

	// ----------------------------------------------------------------------------------------------- //

	#if USE_2D_ATLAS
		BeginSampler(sampler2D,VolumeTexture,VolumeSampler,VolumeTex)
			string	ResourceName =DETAIL_ATLAS_NAME;
			string UIName="Detail Atlas";
			string UIHint="detailmap";
		ContinueSampler(sampler2D,VolumeTexture,VolumeSampler,VolumeTex)
			AddressU  = WRAP;
			AddressV  = WRAP;
			MIPFILTER = NONE;
			MinFilter = LINEAR;
			MagFilter = LINEAR;
		EndSampler;
	#else //USE_2D_ATLAS
		BeginSampler(sampler3D,VolumeTexture,VolumeSampler,VolumeTex)
			string	ResourceName = DETAIL_ATLAS_NAME;
			string UIName="Detail Atlas";
			string UIHint="detailmap";
		ContinueSampler(sampler3D,VolumeTexture,VolumeSampler,VolumeTex)
			AddressU  = WRAP;
			AddressV  = WRAP;
			AddressW = WRAP;
			MIPFILTER = MIPLINEAR;	
		#if __PS3
			MINFILTER=LINEAR;
			MagFilter = LINEAR;
			triLinearthreshold=0;
			MipMapLodBias = -1.;
		#else
			MinFilter = LINEAR;
			MagFilter = LINEAR;
			MipMapLodBias = -0.5;
		#endif
		EndSampler;
	#endif //USE_2D_ATLAS

	#if !USE_SPEC_BLUE_INDEX_MAP
	BeginSampler(sampler2D,DetailIndexTexture,DetailIndexSampler,DetailIndexTex)
			string UIName="Detail Index";
			string UIHint="detailmap";

	ContinueSampler(sampler2D,DetailIndexTexture,DetailIndexSampler,DetailIndexTex)
			AddressU  = CLAMP;
			AddressV  = CLAMP;
			MIPFILTER = POINT;
			MinFilter = POINT;
			MagFilter = POINT;
	EndSampler;
	#endif


	half4 GetDetailMapValue( float2 tCoord, half f )
	{
	#if USE_2D_ATLAS
		float tw=4.;
		int ox = f%4;
		int oy = f/4;
		tCoord = frac( tCoord); // apply wrap
		half3 detv = h3tex2D(VolumeSampler, float2((tCoord.x + ox)/tw, (tCoord.y + oy)/tw));
	#else
		half3 detv = h3tex3D(VolumeSampler, float3(tCoord.x,tCoord.y, f/16));
	#endif
		half3 bump;
		bump.xy= detv.yz*2.-1.;
		// note may need to allow for missing z component.
		return half4( bump.xyz, detv.x);
	}

	half4 GetDetailValue( float2 tCoord, float3 specularValue )
	{
		// Texture Lookup
	#if USE_SPEC_BLUE_INDEX_MAP
		float zv = specularValue.b*16.;
	#else
		float zv = (int)(h1tex2D( DetailIndexSampler, tCoord.xy).r*16);
	#endif
		return GetDetailMapValue( tCoord*detailScale, zv);
	}

	half4 GetDetailValueScaled( float2 tCoord, float3 specularValue, float fDetailScale )
	{
		// Texture Lookup
	#if USE_SPEC_BLUE_INDEX_MAP
		float zv = specularValue.b*16.;
	#else
		float zv = (int)(h1tex2D( DetailIndexSampler, tCoord.xy).r*16);
	#endif
		return GetDetailMapValue( tCoord*fDetailScale, zv);
	}
#else //DETAIL_USE_VOLUME_TEXTURE

#define detailIntensity			(detailSettings.x)
#define detailBumpIntensity		(detailSettings.y)
#define detailScaleUV			(detailSettings.zw)
// ----------------------------------------------------
#define DISABLE_DETAIL 0
#define DOUBLE_DETAIL (__WIN32PC || RSG_ORBIS)
#define DETAIL_USE_SPEC_ALPHA_AS_CONTROL 1

#ifndef UINAME_SAMPLER_DETAILTEXTURE
	#define UINAME_SAMPLER_DETAILTEXTURE		"Detail Map"
#endif
BeginSampler(sampler2D, DetailTexture,DetailSampler,DetailTex)
	string UIName=UINAME_SAMPLER_DETAILTEXTURE;
	string UIHint="detailmap";
	string DontExport="True";
	string TCPTemplate="DetailMap";
	string TextureType="DetailMap";

ContinueSampler(sampler2D,DetailTexture,DetailSampler,DetailTex)
	AddressU  = WRAP;
	AddressV  = WRAP;
	MIPFILTER = MIPLINEAR;
#ifdef USE_ANISOTROPIC
	MinFilter = MINANISOTROPIC;
	MagFilter = MAGLINEAR;
	ANISOTROPIC_LEVEL(4)
#else
	MinFilter = LINEAR;
	MagFilter = LINEAR;
#endif
EndSampler;
	
float4 GetDetailValue( float2 tCoord )
{
	// Texture Lookup
	float3 normal= tex2D_NormalMap(DetailSampler, tCoord*detailScaleUV);
	normal.xy=normal.xy*2.0f-1.f;
	// note may need to allow for missing z component.
	return float4( normal.xyz, normal.x);
}


float3 GetDetailBumpAndIntensity(  float2 IN_texCoord, 	float detailAlpha)
{
#if !DISABLE_DETAIL						
	float4 detv = GetDetailValue( IN_texCoord );

#if DOUBLE_DETAIL 
	detv = detv*.5+ GetDetailValue( IN_texCoord*3.17f )*.5;
#endif
	detv.w=1.+-detv.w*detailIntensity*detailAlpha;
	float3 bumpFactor = float3(0.,0.f, detv.w);
#if NORMAL_MAP
	bumpFactor.xy = detv.xy*detailBumpIntensity*detailAlpha;
#endif
	return bumpFactor;

#else//!DISABLE_DETAIL
	return float3(0.,0.,1.);
#endif
}
/*
void ApplyDetail(  float2 IN_texCoord, 
						#if NORMAL_MAP
							float3 IN_worldTangent, float3 IN_worldBinormal,float3 IN_worldEyePos,
						#endif
						inout SurfaceProperties surfaceInfo, 
						inout StandardLightingProperties surfaceStandardLightingProperties,
						float detailAlpha)
{
#if !DISABLE_DETAIL						
	float4 detv = GetDetailValue( IN_texCoord );

#if DOUBLE_DETAIL 
	detv = detv*.5+ GetDetailValue( IN_texCoord*3.17f )*.5;
#endif
	detv.w=1.+-detv.w*detailIntensity*detailAlpha;
	
	float di = detv.w;
	surfaceStandardLightingProperties.diffuseColor.rgb*=di;

#if NORMAL_MAP
		float2 bumpmap=detv.xy;
		// Only add in tangental components straight up normal doesn't affect it.
		float3 wspaceBump = IN_worldTangent.xyz * bumpmap.x + IN_worldBinormal.xyz* bumpmap.y;
		surfaceInfo.surface_worldNormal.xyz += wspaceBump*detailBumpIntensity*detailAlpha;
		surfaceInfo.surface_worldNormal.xyz = normalize(surfaceInfo.surface_worldNormal.xyz );

	// Turned off as want to try and reduce chance of compiler calculating fresnel twice
	#if SPECULAR && 0 
		float3 temp_eyeDirection = normalize(IN_worldEyePos);	
		float fresnel=specularFresnel;
		surfaceInfo.surface_fresnel=fresnel;
	#endif //SPECULAR

#endif // NORMAL_MAP


#endif //DISABLE_DETAIL
}
*/


#endif  //DETAIL_USE_VOLUME_TEXTURE

#endif  //DETAIL_MAP_FXH
