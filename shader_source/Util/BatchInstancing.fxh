//Grass Batches
#define GRASS_BATCH_NUM_CONSTS_PER_INSTANCE	(3)
#define GRASS_BATCH_SHARED_DATA_COUNT		(8)
#define GRASS_BATCH_CS_CULLING				((1 && (__D3D11 && RSG_PC)) || (1 && RSG_DURANGO) || (1 && RSG_ORBIS)) //(1 && (__SHADERMODEL >= 40))

#define GRASS_BATCH_CS_THREADS_PER_WAVEFRONT 64

#if GRASS_BATCH_CS_CULLING
#	define GRASS_BATCH_CS_CULLING_SWITCH(_if_CS_,_else_) _if_CS_
#else
#	define GRASS_BATCH_CS_CULLING_SWITCH(_if_CS_,_else_) _else_
#endif

#if defined(__SHADERMODEL)
#	define CASCADE_SHADOWS_SCAN_NUM_PLANES 16	//See ScanCascadeShadows.h (Only define for shaders.)

	//Grass batch instanced vert stream is interpreted by the input assembler/fetch shader. I don't think that'll work for the CS.
	struct BatchRawInstanceData //grassBatchRawInstanceData
	{
		uint PosXY_u16;
		uint PosZ_u16_NormXY_u8;
		uint ColorRGB_Scale_u8;
		uint Ao_Pad3_u8;
	};

	struct BatchCSInstanceData
	{
		//BatchRawInstanceData raw;
		uint InstId_u16_CrossFade_Scale_u8;
	};

	//This is now in the raw structured buffer!
	struct BatchInstanceData
	{
		float2 pos				: BLENDWEIGHT;
		float posZ				: TEXCOORD1;
		float2 normal			: TEXCOORD2;
		float4 color_scale		: TEXCOORD3;
		float4 ao_pad3			: TEXCOORD7;
	};

	struct BatchGlobalData
	{
		float4 worldMatA		: TEXCOORD4;
		float4 worldMatB		: TEXCOORD5;
		float4 worldMatC		: TEXCOORD6;
	};

	struct BatchComputeData
	{
		uint InstId;
		float CrossFade;
		float Scale;
	};

	BatchInstanceData ExpandRawInstanceDataBigEndian(BatchRawInstanceData raw)
	{
		BatchInstanceData OUT;
		uint3 pos;
		pos.x = (raw.PosXY_u16 >> 16) & 0xFFFF;
		pos.y = (raw.PosXY_u16) & 0xFFFF;
		pos.z = (raw.PosZ_u16_NormXY_u8 >> 16) & 0xFFFF;

		uint2 normal;
		normal.x = (raw.PosZ_u16_NormXY_u8 >> 8) & 0xFF;
		normal.y = (raw.PosZ_u16_NormXY_u8) & 0xFF;

		uint3 color;
		color.r = (raw.ColorRGB_Scale_u8 >> 24) & 0xFF;
		color.g = (raw.ColorRGB_Scale_u8 >> 16) & 0xFF;
		color.b = (raw.ColorRGB_Scale_u8 >> 8) & 0xFF;

		uint scale = (raw.ColorRGB_Scale_u8) & 0xFF;

		uint aoScale = (raw.Ao_Pad3_u8 >> 24) & 0xFF;

		OUT.pos = ((float2)pos.xy) / 65535.0f;
		OUT.posZ = ((float)pos.z) / 65535.0f;
		OUT.normal = ((float2)normal) / 255.0f;
		OUT.color_scale.rgb = ((float3)color) / 255.0f;
		OUT.color_scale.w = ((float)scale) / 255.0f;
		OUT.ao_pad3 = ((float)aoScale) / 255.0f;

		return OUT;
	}

	BatchInstanceData ExpandRawInstanceData(BatchRawInstanceData raw)
	{
		//Since we are bit-shifting binary data, need to remember endian order...
		BatchInstanceData OUT;
		uint3 pos;
		pos.x = (raw.PosXY_u16) & 0xFFFF;
		pos.y = (raw.PosXY_u16 >> 16) & 0xFFFF;
		pos.z = (raw.PosZ_u16_NormXY_u8) & 0xFFFF;

		uint2 normal;
		normal.x = (raw.PosZ_u16_NormXY_u8 >> 16) & 0xFF;
		normal.y = (raw.PosZ_u16_NormXY_u8 >> 24) & 0xFF;

		uint3 color;
		color.r = (raw.ColorRGB_Scale_u8) & 0xFF;
		color.g = (raw.ColorRGB_Scale_u8 >> 8) & 0xFF;
		color.b = (raw.ColorRGB_Scale_u8 >> 16) & 0xFF;

		uint scale = (raw.ColorRGB_Scale_u8 >> 24) & 0xFF;

		uint aoScale = (raw.Ao_Pad3_u8) & 0xFF;

		OUT.pos = ((float2)pos.xy) / 65535.0f;
		OUT.posZ = ((float)pos.z) / 65535.0f;
		OUT.normal = ((float2)normal) / 255.0f;
		OUT.color_scale.rgb = ((float3)color) / 255.0f;
		OUT.color_scale.w = ((float)scale) / 255.0f;
		OUT.ao_pad3 = ((float)aoScale) / 255.0f;

		return OUT;
	}

	BatchComputeData ExpandComputeData(BatchCSInstanceData csData)
	{
		BatchComputeData OUT;
		OUT.InstId = csData.InstId_u16_CrossFade_Scale_u8 & 0xFFFF;

		uint crossFade =	(csData.InstId_u16_CrossFade_Scale_u8 >> 16) & 0xFF;
		uint scale =		(csData.InstId_u16_CrossFade_Scale_u8 >> 24) & 0xFF;

		OUT.CrossFade =	((float) crossFade) / 255.0f;
		OUT.Scale =		((float) scale) / 255.0f;

		return OUT;
	}

#else
#	define GrassBatchCSInstanceData_Size (sizeof(u32) * 1)
#	define GrassBatchCSInstanceData_Align (4)	//4 is min size for PS4?

#	if GRASS_BATCH_CS_CULLING
#		define GRASS_BATCH_CS_CULLING_ONLY(...)  __VA_ARGS__
#	else
#		define GRASS_BATCH_CS_CULLING_ONLY(...)
#	endif
#endif
