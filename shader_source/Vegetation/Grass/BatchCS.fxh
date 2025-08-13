//
// BatchCS.fxh : Grass Batch Cull Compute Shader
//
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#if RSG_DURANGO
struct IndirectArgBuffer
{
	uint m_indexCountPerInstance;
	uint m_instanceCount;
	uint m_startIndexLocation;
	uint m_baseVertexLocation;
	uint m_startInstanceLocation;
};

RWStructuredBuffer<BatchCSInstanceData> VisibleInstanceBuffer0 REGISTER2(cs_5_0, u0); //register(u0);
RWStructuredBuffer<BatchCSInstanceData> VisibleInstanceBuffer1 REGISTER2(cs_5_0, u1);
RWStructuredBuffer<BatchCSInstanceData> VisibleInstanceBuffer2 REGISTER2(cs_5_0, u2);
RWStructuredBuffer<BatchCSInstanceData> VisibleInstanceBuffer3 REGISTER2(cs_5_0, u3);
RWStructuredBuffer<IndirectArgBuffer> IndirectArgs REGISTER2(cs_5_0, u4);

void GetIndirectArgBuffRange(uint lod, out uint begin, out uint end)
{
	switch(lod)
	{
	case 0:	 begin = 0; end = gIndirectCountPerLod[0]; break;
	case 1:	 begin = gIndirectCountPerLod[0]; end = gIndirectCountPerLod[1]; break;
	case 2:	 begin = gIndirectCountPerLod[1]; end = gIndirectCountPerLod[2]; break;
	case 3:  begin = gIndirectCountPerLod[2]; end = gIndirectCountPerLod[3]; break;
	default: begin = 0; end = 0; break;	//Invalid
	}
}

void AppendToBuffer(BatchCSInstanceData data, uint lod, RWStructuredBuffer<BatchCSInstanceData> buff)
{
	uint begin, end;
	GetIndirectArgBuffRange(lod, begin, end);
	
	if(begin >= end)	//Shouldn't happen b/c this should already be handled by LOD distance param to disable UAV buffers that aren't set.
		return;

	uint dest;
	InterlockedAdd(IndirectArgs[begin].m_instanceCount, 1, dest);
	buff[dest] = data;

	for(uint index = begin + 1; index < end; ++index)
		InterlockedAdd(IndirectArgs[index].m_instanceCount, 1);
}
#else
AppendStructuredBuffer<BatchCSInstanceData> VisibleInstanceBuffer0 REGISTER2(cs_5_0, u0); //register(u0);
AppendStructuredBuffer<BatchCSInstanceData> VisibleInstanceBuffer1 REGISTER2(cs_5_0, u1);
AppendStructuredBuffer<BatchCSInstanceData> VisibleInstanceBuffer2 REGISTER2(cs_5_0, u2);
AppendStructuredBuffer<BatchCSInstanceData> VisibleInstanceBuffer3 REGISTER2(cs_5_0, u3);

#define AppendToBuffer(data, lod, buff) (buff.Append(data))
#endif

#define LOD_COUNT (4)

#define GRASS_CS_TWO_STEP_CROSSFADE (1)

uint GetLastLodIndex()
{
#if !RSG_ORBIS
	[unroll]
#endif
	for(uint i = LOD_COUNT - 1; i > 0; --i)
	{
		if(gLodThresholds[i] >= 0)
			return i;
	}

	return 0;
}

uint CalcLODLevelAndCrossfade(float3 position, uint lastLODIdx, OUT float crossFade)
{
	// check if this drawable contains any LOD data : if so then do LOD selection
	const float dist = length(gCameraPosition - position);

	uint lod = lastLODIdx;
	crossFade = -1.0f;
	if(gLodThresholds[1] >= 0)	//Not really necessary?
	{
		for(uint i = 0; i < lastLODIdx; ++i)
		{
			if(dist < gLodThresholds[i])
			{
				lod = i;

				float fadeDist = gLodThresholds[i] - dist;
				float fadeRange = i == lastLODIdx-1 ? gCrossFadeDistance.y : gCrossFadeDistance.x;

				if(fadeDist < fadeRange)
				{
					crossFade = fadeDist / fadeRange;
				}

				break;
			}
		}
	}

	return lod;
}

uint ComputeLod(float3 position, uint lastLODIdx, OUT float crossFade)
{
	uint lod = CalcLODLevelAndCrossfade(position, lastLODIdx, crossFade);

	if(gLodInstantTransition && crossFade > -1.0f)
	{
		if(crossFade < 0.5f && lod + 1 < LOD_COUNT && gLodThresholds[lod + 1] >= 0.0f)
			lod = lod + 1;
			
		crossFade = -1.0f;
	}

	if(crossFade >= 0.0f)
	{
		//Clamp
		const float minFade = 1.0f/255.0f;
		const float maxFade = 254.0f/255.0f;

		if(crossFade < minFade)
		{
			crossFade = -1.0f;
			if(lod < lastLODIdx)
				lod = lod + 1;
		}
		else if(crossFade > maxFade)
		{
			crossFade = -1.0f;
		}
	}

	return lod;
}

float ComputeLodFade(float3 position, uint nInstIndex)
{
	const float startFadeDist = gLodFadeStartDist[gIsShadowPass] >= 0.0f ? gLodFadeStartDist[gIsShadowPass]  * gLodFadeControlRange.z : gLodFadeControlRange.x;
	const float endFadeDist = gLodFadeControlRange.y;
	const float distFromCamera = length(gCameraPosition - position);
	
	float dist = distFromCamera - startFadeDist;
	float threshold = dist / endFadeDist;

	//Compute "random" reference val for current instance
	const float scaledRange = 1.0f - gLodFadeRange[gIsShadowPass];
	float2 s = sin(position.xy * gLodFadeTileScale);
	float c = cos(s.x + s.y + nInstIndex);
	float ref = (c + 1.0f) * 0.5f;
	ref = pow(ref, gLodFadePower[gIsShadowPass]) * scaledRange;

	return saturate(((ref + gLodFadeRange[gIsShadowPass]) - threshold) / gLodFadeRange[gIsShadowPass]);
}


BatchRawInstanceData CompressInstanceDataBigEndian(BatchInstanceData inst)
{
	BatchRawInstanceData OUT = (BatchRawInstanceData)0;

	uint3 pos = float3(inst.pos.xy, inst.posZ) * 65535.0f;
	uint2 normal = inst.normal * 255.0f;
	uint4 color_scale = inst.color_scale * 255.0f;
	uint aoScale = inst.ao_pad3.x * 255.0f;

	OUT.PosXY_u16			|= (pos.x << 16)			& 0xFFFF0000;
	OUT.PosXY_u16			|=  pos.y					& 0x0000FFFF;
	OUT.PosZ_u16_NormXY_u8	|= (pos.z << 16)			& 0xFFFF0000;

	OUT.PosZ_u16_NormXY_u8	|= (normal.x << 8)			& 0x0000FF00;
	OUT.PosZ_u16_NormXY_u8	|=  normal.y				& 0x000000FF;	

	OUT.ColorRGB_Scale_u8	|= (color_scale.x << 24)	& 0xFF000000;
	OUT.ColorRGB_Scale_u8	|= (color_scale.y << 16)	& 0x00FF0000;
	OUT.ColorRGB_Scale_u8	|= (color_scale.z <<  8)	& 0x0000FF00;
	OUT.ColorRGB_Scale_u8	|=  color_scale.w			& 0x000000FF;

	OUT.Ao_Pad3_u8		|=  (aoScale.x << 24)		& 0xFF000000;

	return OUT;
}

BatchRawInstanceData CompressInstanceData(BatchInstanceData inst)
{
	BatchRawInstanceData OUT = (BatchRawInstanceData)0;

	uint3 pos = float3(inst.pos.xy, inst.posZ) * 65535.0f;
	uint2 normal = inst.normal * 255.0f;
	uint4 color_scale = inst.color_scale * 255.0f;
	uint aoScale = inst.ao_pad3.x * 255.0f;

	OUT.PosXY_u16			|=  pos.x					& 0x0000FFFF;
	OUT.PosXY_u16			|= (pos.y << 16)			& 0xFFFF0000;
	OUT.PosZ_u16_NormXY_u8	|=  pos.z					& 0x0000FFFF;

	OUT.PosZ_u16_NormXY_u8	|= (normal.x << 16)			& 0x00FF0000;
	OUT.PosZ_u16_NormXY_u8	|= (normal.y << 24)			& 0xFF000000;

	OUT.ColorRGB_Scale_u8	|=  color_scale.x			& 0x000000FF;
	OUT.ColorRGB_Scale_u8	|= (color_scale.y <<  8)	& 0x0000FF00;
	OUT.ColorRGB_Scale_u8	|= (color_scale.z << 16)	& 0x00FF0000;
	OUT.ColorRGB_Scale_u8	|= (color_scale.w << 24)	& 0xFF000000;

	OUT.Ao_Pad3_u8		|=  aoScale.x				& 0x000000FF;
	
	return OUT;
}

BatchCSInstanceData CompressComputeData(BatchComputeData csData)
{
	BatchCSInstanceData OUT = (BatchCSInstanceData)0;
	uint crossFade = (csData.CrossFade >= 0.0f ? csData.CrossFade : 1.0f) * 255.0f;
	uint scale = csData.Scale * 255.0f;

	OUT.InstId_u16_CrossFade_Scale_u8	|= csData.InstId		& 0x0000FFFF;
	OUT.InstId_u16_CrossFade_Scale_u8	|= (crossFade << 16)	& 0x00FF0000;
	OUT.InstId_u16_CrossFade_Scale_u8	|= (scale << 24)		& 0xFF000000;

	return OUT;
}

bool IsInstanceInFrustum(float3 worldPos, float scale)
{
	bool inFrustum = true;
	float size = gInstAabbRadius * scale;

	for(uint i = 0; i < gNumClipPlanes; ++i)
	{
		float dist = dot(gClipPlanes[i] * gClipPlaneNormalFlip, float4(worldPos, 1));
		inFrustum = inFrustum && (dist >= -size);
	}

	return inFrustum;
}


[numthreads(GRASS_BATCH_CS_THREADS_PER_WAVEFRONT,1,1)]
void CS_GrassBatchCull(uint3 DTid : SV_DispatchThreadID)
{
	uint numStructs, stride;
	RawInstanceBuffer.GetDimensions(numStructs, stride);
	bool validThread = DTid.x < numStructs;

	const BatchRawInstanceData raw = RawInstanceBuffer[DTid.x];
	BatchInstanceData inst = ExpandRawInstanceData(raw);

	float3 posOffset = float3(inst.pos, inst.posZ) * vecBatchAabbDelta;
	float3 translation = vecBatchAabbMin + posOffset;

	//Compute scale.
	BatchGlobalData global = FetchSharedInstanceVertexData(DTid.x);
	float fade = ComputeLodFade(translation, DTid.x);

	const float perInstScale = ((gScaleRange.y - gScaleRange.x) * inst.color_scale.w) + gScaleRange.x;

	const float scaleRange = gScaleRange.y * gScaleRange.z;
	float randScale = global.worldMatA.w * scaleRange * 2; //*2 b/c art wants scale range to be +/-
	randScale -= scaleRange;

	float finalScale = max(perInstScale + randScale, 0.0f) * fade;
	
#if USE_SKIP_INSTANCE
	bool validInstance = ((DTid.x & 0xf) & (gGrassSkipInstance & 0xf)) != 0;
#else
	bool validInstance = true;
#endif

	if(finalScale > 0 && IsInstanceInFrustum(translation, finalScale) && validThread && validInstance)
	{
		BatchComputeData csData;
		csData.InstId = DTid.x;
		csData.Scale = fade; //finalScale;

		const uint lastLODIdx = GetLastLodIndex();
		uint lod = ComputeLod(translation, lastLODIdx, csData.CrossFade);

		bool isCrossfading = csData.CrossFade >= 0.0f && lod < lastLODIdx;

#if GRASS_CS_TWO_STEP_CROSSFADE
		float crossFade = csData.CrossFade;
		csData.CrossFade = isCrossfading ? saturate(crossFade * 2.0f) : crossFade;	//Rescale fade range to [0.5, 1.0]
#endif //GRASS_CS_TWO_STEP_CROSSFADE
		BatchCSInstanceData OUT = CompressComputeData(csData);
		//OUT.raw = raw;

		if(lod == 0)
		{
			AppendToBuffer(OUT, 0, VisibleInstanceBuffer0); //VisibleInstanceBuffer0.Append(OUT);
		}
		else if(lod == 1)
		{
			AppendToBuffer(OUT, 1, VisibleInstanceBuffer1); //VisibleInstanceBuffer1.Append(OUT);
		}
		else if(lod == 2)
		{
			AppendToBuffer(OUT, 2, VisibleInstanceBuffer2); //VisibleInstanceBuffer2.Append(OUT);
		}
		else
		{
			AppendToBuffer(OUT, 3, VisibleInstanceBuffer3); //VisibleInstanceBuffer3.Append(OUT);
		}

		//NOTE: Keep appends in this order! I tried adding appends below to the if block above which resulted in random flickering. Not sure why yet, seems like it might be
		//		some odd compiler error. Should investigate further.
		if(isCrossfading)
		{
			BatchComputeData lodCsData = csData;
#if GRASS_CS_TWO_STEP_CROSSFADE
			lodCsData.CrossFade = saturate((1.0f - crossFade) * 2.0f);	//Rescale fade range to [0.0, 0.5]
#else //GRASS_CS_TWO_STEP_CROSSFADE
			lodCsData.CrossFade = 1.0f - csData.CrossFade;
#endif //GRASS_CS_TWO_STEP_CROSSFADE
			BatchCSInstanceData lodOUT = CompressComputeData(lodCsData);

			if(lod == 0)
			{
				AppendToBuffer(lodOUT, 1, VisibleInstanceBuffer1); //VisibleInstanceBuffer1.Append(lodOUT);
			}
			else if(lod == 1)
			{
				AppendToBuffer(lodOUT, 2, VisibleInstanceBuffer2); //VisibleInstanceBuffer2.Append(lodOUT);
			}
			else if(lod == 2)
			{
				AppendToBuffer(lodOUT, 3, VisibleInstanceBuffer3); //VisibleInstanceBuffer3.Append(lodOUT);
			}
		}
	}
// 	else
// 	{
// 		uint ajgDbgFlags = 0x80;
// 		if(!IsInstanceInFrustum(translation, finalScale))
// 		{
// 			//inst.color_scale.rgb = float3(1.0f, 0.0f, 0.0f);
// 			ajgDbgFlags = ajgDbgFlags | 0x1;
// 		}
// 		else if(!(finalScale > 0))
// 		{
// 			//inst.color_scale.rgb = float3(0.0f, 0.0f, 1.0f);
// 			ajgDbgFlags = ajgDbgFlags | 0x2;
// 		}
// 		else
// 		{
// 			//inst.color_scale.rgb = float3(255.0f/255.0f, 80.0f/255.0f, 200.0f/255.0f); //Color32(255, 80, 200, 255)
// 			ajgDbgFlags = ajgDbgFlags | 0x3;
// 		}
// 
// 		BatchComputeData csData;
// 		csData.InstId = DTid.x;
// 		csData.CrossFade = 1.0f;
// 		csData.Scale = ajgDbgFlags;
// 
// 		BatchCSInstanceData OUT = CompressComputeData(csData);
// 		VisibleInstanceBuffer0.Append(OUT); //Always append for a PIX test...
// 	}
}

technique BatchCull
{
	pass grass_cull_cs
	{
		SetComputeShader(compileshader(cs_5_0, CS_GrassBatchCull()));
	}
}
