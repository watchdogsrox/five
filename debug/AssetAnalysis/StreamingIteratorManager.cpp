// ================================================
// debug/AssetAnalysis/StreamingIteratorManager.cpp
// (c) 2013 RockstarNorth
// ================================================

#if __BANK

#include "bank/bank.h"
#include "grcore/effect.h"
#include "grcore/image.h"
#include "grcore/texture.h"
#include "grcore/texturedebugflags.h"
#include "grmodel/shader.h"
#include "string/stringutil.h"

#include "fwscene/stores/boxstreamer.h"
#include "fwscene/stores/txdstore.h"
#include "fwscene/stores/dwdstore.h"
#include "fwscene/stores/drawablestore.h"
#include "fwscene/stores/fragmentstore.h"
#include "fwscene/stores/mapdatastore.h"
#include "fwsys/timer.h"
#include "fwutil/orientator.h"
#include "fwutil/xmacro.h"
#include "streaming/streaming.h"
#include "streaming/streamingengine.h"
#include "streaming/packfilemanager.h"
#include "vfx/ptfx/ptfxmanager.h"
#include "vfx/vehicleglass/vehicleglasswindow.h"

#include "fragment/drawable.h"
#include "fragment/type.h"

#include "camera/CamInterface.h"
#include "camera/debug/DebugDirector.h"
#include "camera/helpers/Frame.h"
#include "debug/AssetAnalysis/AssetAnalysis.h"
#include "debug/AssetAnalysis/AssetAnalysisUtil.h"
#include "debug/AssetAnalysis/GeometryCollector.h"
#include "debug/AssetAnalysis/StreamingIterator.h"
#include "debug/AssetAnalysis/StreamingIteratorManager.h"
#include "debug/DebugArchetypeProxy.h"
#include "debug/DebugGeometryUtil.h"
#include "modelinfo/BaseModelInfo.h"
#include "modelinfo/MloModelInfo.h"
#include "Objects/DummyObject.h"
#include "Peds/Ped.h"
#include "Peds/PlayerInfo.h"
#include "physics/gtaArchetype.h"
#include "scene/Entity.h"
#include "scene/AnimatedBuilding.h"

enum
{
	SI_FILTER_FLAG_SKIP_VEHICLES         = BIT(0),
	SI_FILTER_FLAG_SKIP_PEDS             = BIT(1),
	SI_FILTER_FLAG_SKIP_PROPS            = BIT(2),
	SI_FILTER_FLAG_SKIP_INTERIORS        = BIT(3),
	SI_FILTER_FLAG_SKIP_MAPDATA_HD       = BIT(4),
	SI_FILTER_FLAG_SKIP_MAPDATA_NONHD    = BIT(5),
	SI_FILTER_FLAG_SKIP_MAPDATA_INTERIOR = BIT(6),
	SI_FILTER_FLAG_SKIP_MAPDATA_EXTERIOR = BIT(7),
	SI_FILTER_FLAG_SKIP_MAPDATA_SCRIPT   = BIT(8),
	SI_FILTER_FLAG_SKIP_MAPDATA_LODLIGHT = BIT(9),
	SI_FILTER_FLAG_SKIP_Z_Z_ASSETS       = BIT(10),
};

static u32             g_siTimeStarted                    = 0;
static fiStream*       g_siLog                            = NULL;
static bool            g_siEnabled                        = false;
static bool            g_siQueueMapDataIterator           = false; // run mapdata iterator last
static bool            g_siBreakWhenFinished              = false; // break in debugger when finished
static bool            g_siShowInfo                       = false;
static char            g_siFilterRpfPath[80]              = "";
static char            g_siFilterAssetName[80]            = "";
static u32             g_siFilterSkipFlags                = /*SI_FILTER_FLAG_SKIP_MAPDATA_SCRIPT |*/ SI_FILTER_FLAG_SKIP_MAPDATA_LODLIGHT | SI_FILTER_FLAG_SKIP_Z_Z_ASSETS;
static Vec3V           g_siFilterMapDataPhysicalBoundsMin = Vec3V(-16000.0f, -16000.0f, -16000.0f);
static Vec3V           g_siFilterMapDataPhysicalBoundsMax = Vec3V(+16000.0f, +16000.0f, +16000.0f);
static atMap<u32,bool> g_siTestHDModels;
static atMap<u32,bool> g_siTestSkipDrawableNames;
static bool            g_siTestSkipShadowProxies          = false;
static int             g_siTestDrawableNumTris            = 0;
static u32             g_siTestDrawableFlags              = 0;
static u32             g_siTestFragmentFlags              = 0;
static u32             g_siTestMaterialFlags              = 0;
static u32             g_siTestTextureFlags               = 0;
static int             g_siTestTextureMinSize             = 1024;
static int             g_siTestTextureMaxSize             = 8192;
static char            g_siTestTextureNameFilter[80]      = "";
static fiStream*       g_siTestCSV                        = NULL;
static char            g_siTestCSVOutputPath[80]          = "assets:/non_final/test.csv";

// TODO -- fix leak in script mapdata .. Ian says to use REQUEST_IPL and REMOVE_IPL script commands
// for now, we can run with '-numStreamedSlots=50000' in the commandline to get around this problem

enum
{
	SI_TEST_DRAWABLE_NUM_TRIS          = BIT(0),
	SI_TEST_DRAWABLE_ORIENTATION       = BIT(1),
	SI_TEST_DRAWABLE_LODS              = BIT(2),
	SI_TEST_DRAWABLE_WATER_FLOW        = BIT(3),
	SI_TEST_DRAWABLE_CABLE_SHADER_VARS = BIT(4),
	SI_TEST_DRAWABLE_CABLE_NORMALS     = BIT(5),
	SI_TEST_DRAWABLE_HD_WITH_SUBMESHES = BIT(6),
	SI_TEST_DRAWABLE_MULTIPLE_BUCKETS  = BIT(7),
};

enum
{
	SI_TEST_FRAGMENT_WINDOWS = BIT(0),
};

enum
{
	SI_TEST_MATERIAL_LOW_FRESNEL       = BIT(0),
	SI_TEST_MATERIAL_NO_DIFFUSE_TEX    = BIT(1),
	SI_TEST_MATERIAL_SINGLE_PARAM_TINT = BIT(2),
};

enum
{
	SI_TEST_TEXTURE_SIZE     = BIT(0),
	SI_TEST_TEXTURE_MIP_SIZE = BIT(1),
	SI_TEST_TEXTURE_NON_POW2 = BIT(2),
	SI_TEST_TEXTURE_PATHS    = BIT(3),
};

static void StreamingIteratorTestDrawableNumTris(const Drawable* drawable, const char* drawablePath)
{
	const int numTris = GeometryUtil::CountTrianglesForDrawable(drawable, 0, drawablePath);

	if (g_siTestDrawableNumTris < numTris)
	{
		g_siTestDrawableNumTris = numTris;
		Displayf("### new maximum: %s has %d triangles", drawablePath, numTris);
	}
}

static void StreamingIteratorTestDrawableOrientation(const Drawable* drawable, const char* drawablePath)
{
	if (g_siTestCSV == NULL)
	{
		g_siTestCSV = fiStream::Create(g_siTestCSVOutputPath);
	}

	fwOrientator o;
//	o.ConstructFromDrawable(drawable, drawablePath);
	{
		static fwOrientator& _this = o;
		class AddTriangle { public: static void func(Vec3V_In v0, Vec3V_In v1, Vec3V_In v2, int, int, int, void*)
		{
			_this.AddPoint(v0);
			_this.AddPoint(v1);
			_this.AddPoint(v2);
		}};

		o.Begin();
		GeometryUtil::AddTrianglesForDrawable(drawable, LOD_HIGH, drawablePath, NULL, AddTriangle::func);
		o.Process();
	}

	if (g_siTestCSV)
	{
		o.WriteToCSV(g_siTestCSV, drawablePath);
	}
}

static void StreamingIteratorTestDrawableLODs(const Drawable* drawable, const char* drawablePath)
{
	if (g_siTestCSV == NULL)
	{
		g_siTestCSV = fiStream::Create(g_siTestCSVOutputPath);
	}

	const rmcLodGroup& group = drawable->GetLodGroup();
	char str[512] = "";
	sprintf(str, "%s,", drawablePath);

	for (int lodIndex = 0; lodIndex < LOD_COUNT; lodIndex++)
	{
		if (group.ContainsLod(lodIndex))
		{
			const rmcLod& lod = group.GetLod(lodIndex);
			strcat(str, atVarString(",lod%d[", lodIndex).c_str());

			if (lod.GetCount() != 1)
			{
				char temp[512] = "";
				sprintf(temp, "%s,lod[%d] has %d models", drawablePath, lodIndex, lod.GetCount());

				if (g_siTestCSV)
				{
					fprintf(g_siTestCSV, "%s\n", temp);
				}

				Displayf("%s", temp);
			}

			for (int lodModelIndex = 0; lodModelIndex < lod.GetCount(); lodModelIndex++)
			{
				const grmModel* pModel = lod.GetModel(lodModelIndex);

				if (pModel)
				{
					const grmModel& model = *pModel;
					strcat(str, lod.GetCount() > 1 ? "(" : "");

					for (int geomIndex = 0; geomIndex < model.GetGeometryCount(); geomIndex++)
					{
						const grmGeometry& geom = model.GetGeometry(geomIndex);
						strcat(str, atVarString("%s%d", geomIndex ? "-" : "", geom.GetPrimitiveCount()).c_str());
					}

					strcat(str, lod.GetCount() > 1 ? ")" : "");
				}
				else
				{
					char temp[512] = "";
					sprintf(temp, "%s,lod[%d] model %d is NULL", drawablePath, lodIndex, lodModelIndex);

					if (g_siTestCSV)
					{
						fprintf(g_siTestCSV, "%s\n", temp);
					}

					Displayf("%s", temp);
				}
			}

			strcat(str, "]");
		}
	}

	if (g_siTestCSV)
	{
		fprintf(g_siTestCSV, "%s\n", str);
	}
}

template <typename T> static void StreamingIteratorTestDrawableWaterFlow_T(T& store, int slot, int entry, const Drawable* drawable, const char* drawablePath)
{
	if (g_siTestCSV == NULL)
	{
		g_siTestCSV = fiStream::Create(g_siTestCSVOutputPath);
	}

	const grmShaderGroup& shaderGroup = drawable->GetShaderGroup();

	for (int i = 0; i < shaderGroup.GetCount(); i++)
	{
		const grmShader& shader = shaderGroup.GetShader(i);

		if (strstr(shader.GetName(), "water"))
		{
			grcEffectVar idVarFlowTexture = shader.LookupVar("FlowTexture");

			if (idVarFlowTexture)
			{
				grcTexture* flowTexture = NULL;
				shader.GetVar(idVarFlowTexture, flowTexture);

				if (flowTexture)
				{
					const u16 templateType = flowTexture->GetTemplateType();

					//if ((templateType & TEXTURE_TEMPLATE_TYPE_MASK) != TEXTURE_TEMPLATE_TYPE_WATERFLOW)
					{
						const atString texturePath = AssetAnalysis::GetTexturePath(store, slot, entry, drawable, flowTexture);
						const bool bIsVRAM = (flowTexture->GetGcmTexture().location == CELL_GCM_LOCATION_LOCAL);
						char temp[512] = "";
						sprintf(temp, "%s/%s[%d],%s,%s,%s", drawablePath, shader.GetName(), i, texturePath.c_str(), GetTextureTemplateTypeString(templateType).c_str(), bIsVRAM ? "VRAM" : "");

						if (g_siTestCSV)
						{
							fprintf(g_siTestCSV, "%s\n", temp);
						}

						Displayf("%s", temp);
					}
				}
			}
		}
	}
}

static void StreamingIteratorTestDrawableCableShaderVars(const Drawable* drawable, const char* drawablePath)
{
	const grmShaderGroup& shaderGroup = drawable->GetShaderGroup();

	Vec3V maxDiffuse(V_ZERO);
	Vec3V maxDiffuse2(V_ZERO);
	float maxEmissive = 0.0f;

	for (int i = 0; i < shaderGroup.GetCount(); i++)
	{
		const grmShader& shader = shaderGroup.GetShader(i);

		if (strcmp(shader.GetName(), "cable") == 0)
		{
			grcEffectVar idVarCableDiffuse = shader.LookupVar("CableDiffuse");
			if (idVarCableDiffuse)
			{
				Vec3V diffuse(V_ZERO);
				shader.GetVar(idVarCableDiffuse, RC_VECTOR3(diffuse));

				maxDiffuse = Max(diffuse, maxDiffuse);
			}

			grcEffectVar idVarCableDiffuse2 = shader.LookupVar("CableDiffuse2");
			if (idVarCableDiffuse2)
			{
				Vec3V diffuse2(V_ZERO);
				shader.GetVar(idVarCableDiffuse2, RC_VECTOR3(diffuse2));

				maxDiffuse2 = Max(diffuse2, maxDiffuse2);
			}

			grcEffectVar idVarCableEmissive = shader.LookupVar("CableEmissive");
			if (idVarCableEmissive)
			{
				float emissive = 0.0f;
				shader.GetVar(idVarCableEmissive, emissive);

				maxEmissive = Max<float>(emissive, maxEmissive);
			}
		}
	}

	if (IsEqualAll(maxDiffuse, Vec3V(V_ZERO)) == 0 ||
		IsEqualAll(maxDiffuse2, Vec3V(V_ZERO)) == 0 ||
		maxEmissive != 0.0f)
	{
		CStreamingIteratorManager::Logf("### %s is lit cable (diffuse=%f,%f,%f, diffuse2=%f,%f,%f, emissive=%f)", drawablePath, VEC3V_ARGS(maxDiffuse), VEC3V_ARGS(maxDiffuse2), maxEmissive);
	}
}

static void StreamingIteratorTestDrawableCableNormals(const Drawable* drawable, const char* drawablePath)
{
	if (g_siTestCSV == NULL)
	{
		g_siTestCSV = fiStream::Create(g_siTestCSVOutputPath);
	}

	const grmShaderGroup& shaderGroup = drawable->GetShaderGroup();
	bool bHasCable = false;

	for (int i = 0; i < shaderGroup.GetCount(); i++)
	{
		const grmShader& shader = shaderGroup.GetShader(i);

		if (strcmp(shader.GetName(), "cable") == 0)
		{
			bHasCable = true;
			break;
		}
	}

	if (bHasCable)
	{
		const rmcLodGroup& lodGroup = drawable->GetLodGroup();

		if (lodGroup.ContainsLod(LOD_HIGH))
		{
			const rmcLod& lod = lodGroup.GetLod(LOD_HIGH);

			int numTriangles      = 0;
			int numTrisTotal      = 0;
			int numSingularities  = 0;
			int numPositiveArea   = 0;
			int numNormalMismatch = 0;
			int numNormalError    = 0;

			float maxAngle = 0.0f;
			float avgAngle = 0.0f;

			for (int lodModelIndex = 0; lodModelIndex < lod.GetCount(); lodModelIndex++)
			{
				const grmModel* pModel = lod.GetModel(lodModelIndex);

				if (pModel)
				{
					for (int geomIndex = 0; geomIndex < pModel->GetGeometryCount(); geomIndex++)
					{
						const grmGeometry& geom = pModel->GetGeometry(geomIndex);
						const grmShader& shader = drawable->GetShaderGroup().GetShader(pModel->GetShaderIndex(geomIndex));

						if (strcmp(shader.GetName(), "cable") == 0 && geom.GetType() == grmGeometry::GEOMETRYQB)
						{
							const grcIndexBuffer* pIndexBuffer = const_cast<grmGeometry&>(geom).GetIndexBuffer();
							const grcVertexBuffer* pVertexBuffer = const_cast<grmGeometry&>(geom).GetVertexBuffer();
							const u16* indices = pIndexBuffer->LockRO();
							const int numIndices = pIndexBuffer->GetIndexCount();

							PS3_ONLY(++g_AllowVertexBufferVramLocks); // PS3: attempting to edit VBs in VRAM
							grcVertexBufferEditor vertexBufferEditor(const_cast<grcVertexBuffer*>(pVertexBuffer), true, true);
							const int vertexCount = vertexBufferEditor.GetVertexBuffer()->GetVertexCount();

							Assert(geom.GetPrimitiveType() == drawTris);
							Assert(geom.GetPrimitiveCount()*3 == (u32)pIndexBuffer->GetIndexCount());

							for (int i = 0; i < numIndices; i += 3)
							{
								const u16 index0 = indices[i + 0];
								const u16 index1 = indices[i + 1];
								const u16 index2 = indices[i + 2];

								if (Max<u16>(index0, index1, index2) >= vertexCount)
								{
									continue; // ps3 vram error?
								}

								const Vec3V p0 = VECTOR3_TO_VEC3V(vertexBufferEditor.GetPosition(index0));
								const Vec3V p1 = VECTOR3_TO_VEC3V(vertexBufferEditor.GetPosition(index1));
								const Vec3V p2 = VECTOR3_TO_VEC3V(vertexBufferEditor.GetPosition(index2));

								const Vec3V n0 = VECTOR3_TO_VEC3V(vertexBufferEditor.GetNormal(index0));
								const Vec3V n1 = VECTOR3_TO_VEC3V(vertexBufferEditor.GetNormal(index1));
								const Vec3V n2 = VECTOR3_TO_VEC3V(vertexBufferEditor.GetNormal(index2));

								Vec3V dv(V_ZERO);
								Vec3V na(V_ZERO);
								Vec3V nb(V_ZERO);
								Vec3V nc(V_ZERO);

								numTrisTotal++; // regardless of singularity or positive-area error

								if (IsEqualAll(p0, p1) && IsEqualAll(p1, p2))
								{
									numSingularities++; // triangles should not be singularities, this is an error (triangle will not be visible)
									continue;
								}
								else if (IsEqualAll(p0, p1)) { dv = Normalize(p0 - p2); na = n0; nb = n1; nc = n2; }
								else if (IsEqualAll(p1, p2)) { dv = Normalize(p1 - p0); na = n1; nb = n2; nc = n0; }
								else if (IsEqualAll(p2, p0)) { dv = Normalize(p2 - p1); na = n2; nb = n0; nc = n1; }
								else
								{
									numPositiveArea++; // triangles should not have actual positive area, this is an error (unless they're doing something special)
									continue;
								}

								if (!IsEqualAll(na, nb))
								{
									numNormalMismatch++;
								}

								const float minAbsDot = Min(Abs(Dot(na, dv)), Abs(Dot(nb, dv)), Abs(Dot(nc, dv))).Getf();
								const float angle     = RtoD*acosf(Clamp<float>(minAbsDot, 0.0f, 1.0f));

								if (angle > 35.0f) // this is pretty tolerant .. really we don't want to see anything near this bad
								{
									numNormalError++;
								}

								maxAngle = Max<float>(angle, maxAngle);
								avgAngle += angle;
								numTriangles++;
							}

							PS3_ONLY(--g_AllowVertexBufferVramLocks); // PS3: finished with editing VBs in VRAM
							pIndexBuffer->UnlockRO();
						}
					}
				}
			}

			char temp[512] = "";
			sprintf(temp, "%s,%d", drawablePath, numTriangles);

			if (numSingularities  > 0 && numTrisTotal > 0) { strcat(temp, atVarString(",singularities=%d (%.2f%%)", numSingularities , 100.0f*(float)numSingularities /(float)numTrisTotal).c_str()); } else { strcat(temp, ",-"); }
			if (numPositiveArea   > 0 && numTrisTotal > 0) { strcat(temp, atVarString(",posarea=%d (%.2f%%)"      , numPositiveArea  , 100.0f*(float)numPositiveArea  /(float)numTrisTotal).c_str()); } else { strcat(temp, ",-"); }
			if (numNormalMismatch > 0 && numTriangles > 0) { strcat(temp, atVarString(",normmismatch=%d (%.2f%%)" , numNormalMismatch, 100.0f*(float)numNormalMismatch/(float)numTriangles).c_str()); } else { strcat(temp, ",-"); }
			if (numNormalError    > 0 && numTriangles > 0) { strcat(temp, atVarString(",normerror=%d (%.2f%%)"    , numNormalError   , 100.0f*(float)numNormalError   /(float)numTriangles).c_str()); } else { strcat(temp, ",-"); }

			strcat(temp, atVarString(",%f", maxAngle).c_str());
			strcat(temp, atVarString(",%f", numTriangles ? avgAngle/(float)numTriangles : 0.0f).c_str());

			if (g_siTestCSV)
			{
				fprintf(g_siTestCSV, "%s\n", temp);
			}

			Displayf("%s", temp);
		}
	}
}

static void StreamingIteratorTestDrawableHDWithSubmeshes(const Drawable* drawable, const char* drawablePath)
{
	if (g_siTestCSV == NULL)
	{
		g_siTestCSV = fiStream::Create(g_siTestCSVOutputPath);
	}

	char modelName[256] = "";
	const char* s = strrchr(drawablePath, '/');

	if (s)
	{
		strcpy(modelName, s + 1);
		char* s2 = strchr(modelName, '.');
		if (s2) { *s2 = '\0'; }
	}

	if (modelName[0] == '\0' || g_siTestHDModels.Access(atHashString(modelName).GetHash()) == NULL)
	{
		return; // skip it, it's not an HD model
	}

	char renderBucketMaskStr[2048] = "";
	char renderBucketWarning[256] = "";

	extern void TextureViewer_GetRenderBucketInfoStrings(const Drawable* pDrawable, u16 phaseVisMask, char* renderBucketMaskStr, int renderBucketMaskStrMaxLen, char* renderBucketWarning, int renderBucketWarningMaxLen);
	TextureViewer_GetRenderBucketInfoStrings(drawable, 0, renderBucketMaskStr, (int)sizeof(renderBucketMaskStr), renderBucketWarning, (int)sizeof(renderBucketWarning));

	if (strstr(renderBucketMaskStr, "[submesh"))
	{
		char temp[512] = "";
		sprintf(temp, "%s,%s", drawablePath, renderBucketMaskStr);

		if (g_siTestCSV)
		{
			fprintf(g_siTestCSV, "%s\n", temp);
		}

		Displayf("%s", temp);
	}
}

static void StreamingIteratorTestDrawableMultipleBuckets(const Drawable* drawable, const char* drawablePath)
{
	if (g_siTestCSV == NULL)
	{
		g_siTestCSV = fiStream::Create(g_siTestCSVOutputPath);
	}

	char renderBucketMaskStr[2048] = "";
	char renderBucketWarning[256] = "";

	extern void TextureViewer_GetRenderBucketInfoStrings(const Drawable* pDrawable, u16 phaseVisMask, char* renderBucketMaskStr, int renderBucketMaskStrMaxLen, char* renderBucketWarning, int renderBucketWarningMaxLen);
	TextureViewer_GetRenderBucketInfoStrings(drawable, 0, renderBucketMaskStr, (int)sizeof(renderBucketMaskStr), renderBucketWarning, (int)sizeof(renderBucketWarning));

	if (strstr(renderBucketMaskStr, "[submesh=0:"))
	{
		int numG = 0;
		int numS = 0;
		int numR = 0;
		int numM = 0;
		int numW = 0;

		for (int i = 0; i < 10; i++)
		{
			char s0[16] = "";
			sprintf(s0, "[submesh=%d:", i);
			const char* s = strstr(renderBucketMaskStr, s0);

			if (s)
			{
				char temp[64] = "";
				strcpy(temp, s + strlen(s0));
				temp[5] = '\0';

				if (strchr(temp, 'g') || strchr(temp, 'G')) { numG++; }
				if (strchr(temp, 's') || strchr(temp, 'S')) { numS++; }
				if (strchr(temp, 'r') || strchr(temp, 'R')) { numR++; }
				if (strchr(temp, 'm') || strchr(temp, 'M')) { numM++; }
				if (strchr(temp, 'w') || strchr(temp, 'W')) { numW++; }
			}
			else
			{
				break;
			}
		}

		if (numG > 1 ||
			numS > 1 ||
			numR > 1 ||
			numM > 1 ||
			numW > 1)
		{
			char temp[512] = "";
			sprintf(temp, "%s,%s", drawablePath, renderBucketMaskStr);

			if (g_siTestCSV)
			{
				fprintf(g_siTestCSV, "%s\n", temp);
			}

			Displayf("%s", temp);
		}
	}
}

static void StreamingIteratorTestMaterialLowFresnel(const grmShader* shader, const char* shaderPath)
{
	if (g_siTestCSV == NULL)
	{
		g_siTestCSV = fiStream::Create(g_siTestCSVOutputPath);
	}

	grcEffectVar idVarFresnel = shader->LookupVar("Fresnel");
	if (idVarFresnel)
	{
		float fresnel = 0.0f;
		shader->GetVar(idVarFresnel, fresnel);

		if (fresnel < 0.7f)
		{
			char temp[512] = "";
			sprintf(temp, "%s,%f", shaderPath, fresnel);

			if (g_siTestCSV)
			{
				fprintf(g_siTestCSV, "%s\n", temp);
			}

			Displayf("%s", temp);
		}
	}
}

static void StreamingIteratorTestMaterialNoDiffuseTex(const grmShader* shader, const char* shaderPath)
{
	if (strstr(shaderPath, "models/farlods.idd"))
	{
		return;
	}

	if (g_siTestCSV == NULL)
	{
		g_siTestCSV = fiStream::Create(g_siTestCSVOutputPath);
	}

	grcEffectVar idVarDiffuseTex = shader->LookupVar("DiffuseTex");
	if (idVarDiffuseTex)
	{
		grcTexture* diffuseTex = NULL;
		shader->GetVar(idVarDiffuseTex, diffuseTex);

		if (diffuseTex == NULL)
		{
			char temp[512] = "";
			sprintf(temp, "%s", shaderPath);

			if (g_siTestCSV)
			{
				fprintf(g_siTestCSV, "%s\n", temp);
			}

			Displayf("%s", temp);
		}
	}
}

static void StreamingIteratorTestMaterialSingleParamTint(const grmShader* shader, const char* shaderPath)
{
	if (g_siTestCSV == NULL)
	{
		g_siTestCSV = fiStream::Create(g_siTestCSVOutputPath);
	}

	grcEffectVar idVarSingleParamTint = shader->LookupVar("SingleParamTint");
	if (idVarSingleParamTint)
	{
		Vec3V singleParamTint = Vec3V(V_ONE);
		shader->GetVar(idVarSingleParamTint, RC_VECTOR3(singleParamTint));

		if (IsEqualAll(singleParamTint, Vec3V(V_ONE)) == 0)
		{
			char temp[512] = "";
			sprintf(temp, "%s,[%f %f %f]", shaderPath, VEC3V_ARGS(singleParamTint));

			if (g_siTestCSV)
			{
				fprintf(g_siTestCSV, "%s\n", temp);
			}

			Displayf("%s", temp);
		}
	}
}

static void StreamingIteratorTestTextureSize(const grcTexture* texture, const char* path)
{
	if (g_siTestCSV == NULL)
	{
		g_siTestCSV = fiStream::Create(g_siTestCSVOutputPath);
	}

	const int w0 = texture->GetWidth();
	const int h0 = texture->GetHeight();

	if (Max<int>(w0, h0) >= g_siTestTextureMinSize &&
		Max<int>(w0, h0) <= g_siTestTextureMaxSize)
	{
		char temp[512] = "";
		sprintf(temp, "%s,%s %dx%d with %d mips", path, grcImage::GetFormatString((grcImage::Format)texture->GetImageFormat()), w0, h0, texture->GetMipMapCount());

		if (g_siTestCSV)
		{
			fprintf(g_siTestCSV, "%s\n", temp);
		}

		Displayf("%s", temp);
	}
}

static void StreamingIteratorTestTextureMipSize(const grcTexture* texture, const char* path)
{
	if (g_siTestCSV == NULL)
	{
		g_siTestCSV = fiStream::Create(g_siTestCSVOutputPath);
	}

	const int w0 = texture->GetWidth();
	const int h0 = texture->GetHeight();
	const int w1 = Max<int>(1, w0>>(texture->GetMipMapCount() - 1));
	const int h1 = Max<int>(1, h0>>(texture->GetMipMapCount() - 1));

	if (Min<int>(w0, h0) >= 32 && Max<int>(w1, h1) < 32)
	{
		char temp[512] = "";
		sprintf(temp, "%s,%s %dx%d with %d mips,smallest mip is %dx%d", path, grcImage::GetFormatString((grcImage::Format)texture->GetImageFormat()), w0, h0, texture->GetMipMapCount(), w1, h1);

		if (g_siTestCSV)
		{
			fprintf(g_siTestCSV, "%s\n", temp);
		}

		Displayf("%s", temp);
	}
}

static void StreamingIteratorTestTextureNonPow2(const grcTexture* texture, const char* path)
{
	if (g_siTestCSV == NULL)
	{
		g_siTestCSV = fiStream::Create(g_siTestCSVOutputPath);
	}

	const int w0 = texture->GetWidth();
	const int h0 = texture->GetHeight();

	if ((w0 & (w0 - 1)) |
		(h0 & (h0 - 1)))
	{
		char temp[512] = "";
		sprintf(temp, "%s,%s %dx%d with %d mips", path, grcImage::GetFormatString((grcImage::Format)texture->GetImageFormat()), w0, h0, texture->GetMipMapCount());

		if (g_siTestCSV)
		{
			fprintf(g_siTestCSV, "%s\n", temp);
		}

		Displayf("%s", temp);
	}
}

static void StreamingIteratorTestTexturePaths(const grcTexture* texture, const char* path)
{
	if (g_siTestCSV == NULL)
	{
		g_siTestCSV = fiStream::Create(g_siTestCSVOutputPath);
	}

	const int w0 = texture->GetWidth();
	const int h0 = texture->GetHeight();

	char temp[512] = "";
	sprintf(temp, "%s,%s %dx%d with %d mips", path, grcImage::GetFormatString((grcImage::Format)texture->GetImageFormat()), w0, h0, texture->GetMipMapCount());

	if (g_siTestCSV)
	{
		fprintf(g_siTestCSV, "%s\n", temp);
	}

	Displayf("%s", temp);
}

template <typename T> static void StreamingIteratorTestTxd_T(T& store, int slot, int entry, const fwTxd* txd)
{
	if (txd)
	{
		for (int i = 0; i < txd->GetCount(); i++)
		{
			const grcTexture* texture = txd->GetEntry(i);

			if (texture)
			{
				if (g_siTestTextureNameFilter[0] == '\0' || stristr(texture->GetName(), g_siTestTextureNameFilter))
				{
					const atString texturePath = atVarString("%s/%s", AssetAnalysis::GetAssetPath(store, slot, entry).c_str(), AssetAnalysis::GetTextureName(texture).c_str());

					if (g_siTestTextureFlags & SI_TEST_TEXTURE_SIZE)
					{
						StreamingIteratorTestTextureSize(texture, texturePath.c_str());
					}

					if (g_siTestTextureFlags & SI_TEST_TEXTURE_MIP_SIZE)
					{
						StreamingIteratorTestTextureMipSize(texture, texturePath.c_str());
					}

					if (g_siTestTextureFlags & SI_TEST_TEXTURE_NON_POW2)
					{
						StreamingIteratorTestTextureNonPow2(texture, texturePath.c_str());
					}

					if (g_siTestTextureFlags & SI_TEST_TEXTURE_PATHS)
					{
						StreamingIteratorTestTexturePaths(texture, texturePath.c_str());
					}
				}
			}
		}
	}
}

template <typename T> static void StreamingIteratorTestMaterial_T(T& store, int slot, int entry, const grmShader* shader, int shaderIndex)
{
	if (shader)
	{
		const atString shaderPath = atVarString("%s/%s[%d]", AssetAnalysis::GetAssetPath(store, slot, entry).c_str(), shader->GetName(), shaderIndex);

		if (g_siTestMaterialFlags & SI_TEST_MATERIAL_LOW_FRESNEL)
		{
			StreamingIteratorTestMaterialLowFresnel(shader, shaderPath.c_str());
		}

		if (g_siTestMaterialFlags & SI_TEST_MATERIAL_NO_DIFFUSE_TEX)
		{
			StreamingIteratorTestMaterialNoDiffuseTex(shader, shaderPath.c_str());
		}

		if (g_siTestMaterialFlags & SI_TEST_MATERIAL_SINGLE_PARAM_TINT)
		{
			StreamingIteratorTestMaterialSingleParamTint(shader, shaderPath.c_str());
		}
	}
}

static void StreamingIteratorTestDrawable_Init()
{
	if (g_siTestSkipShadowProxies)
	{
		static bool bOnce = true;

		if (bOnce)
		{
			bOnce = false;

			for (u32 i = 0; i < CDebugArchetype::GetMaxDebugArchetypeProxies(); i++)
			{
				const CDebugArchetypeProxy* modelInfo = CDebugArchetype::GetDebugArchetypeProxy(i);

				if (modelInfo->GetIsShadowProxy())
				{
					g_siTestSkipDrawableNames[modelInfo->GetHashKey()] = true;
				}
			}
		}
	}
}

template <typename T> static void StreamingIteratorTestDrawable_T(T& store, int slot, int entry, const Drawable* drawable)
{
	StreamingIteratorTestDrawable_Init();

	if (drawable)
	{
		const atString drawablePath = AssetAnalysis::GetAssetPath(store, slot, entry);

		if (g_siTestSkipShadowProxies)
		{
			const char* temp = strrchr(drawablePath.c_str(), '/');

			if (temp)
			{
				char drawableName[512] = "";
				strcpy(drawableName, temp + 1);

				if (strrchr(drawableName, '.'))
					strrchr(drawableName, '.')[0] = '\0';

				if (g_siTestSkipDrawableNames.Access(atHashString(drawableName).GetHash()))
				{
					return;
				}
			}
		}

		if (g_siTestDrawableFlags & SI_TEST_DRAWABLE_NUM_TRIS)
		{
			StreamingIteratorTestDrawableNumTris(drawable, drawablePath.c_str());
		}

		if (g_siTestDrawableFlags & SI_TEST_DRAWABLE_ORIENTATION)
		{
			StreamingIteratorTestDrawableOrientation(drawable, drawablePath.c_str());
		}

		if (g_siTestDrawableFlags & SI_TEST_DRAWABLE_LODS)
		{
			StreamingIteratorTestDrawableLODs(drawable, drawablePath.c_str());
		}

		if (g_siTestDrawableFlags & SI_TEST_DRAWABLE_WATER_FLOW)
		{
			StreamingIteratorTestDrawableWaterFlow_T(store, slot, entry, drawable, drawablePath.c_str());
		}

		if (g_siTestDrawableFlags & SI_TEST_DRAWABLE_CABLE_SHADER_VARS)
		{
			StreamingIteratorTestDrawableCableShaderVars(drawable, drawablePath.c_str());
		}

		if (g_siTestDrawableFlags & SI_TEST_DRAWABLE_CABLE_NORMALS)
		{
			StreamingIteratorTestDrawableCableNormals(drawable, drawablePath.c_str());
		}

		if (g_siTestDrawableFlags & SI_TEST_DRAWABLE_HD_WITH_SUBMESHES)
		{
			StreamingIteratorTestDrawableHDWithSubmeshes(drawable, drawablePath.c_str());
		}

		if (g_siTestDrawableFlags & SI_TEST_DRAWABLE_MULTIPLE_BUCKETS)
		{
			StreamingIteratorTestDrawableMultipleBuckets(drawable, drawablePath.c_str());
		}

		// ===

		if (g_siTestMaterialFlags)
		{
			const grmShaderGroup* pShaderGroup = drawable->GetShaderGroupPtr();

			if (pShaderGroup)
			{
				u8* shaderFlags = NULL;

				if (g_siTestSkipShadowProxies)
				{
					const int shaderFlagsSize = (pShaderGroup->GetCount() + 7)/8;
					shaderFlags = rage_new u8[shaderFlagsSize];
					sysMemSet(shaderFlags, 0, shaderFlagsSize);

					const rmcLodGroup& lodGroup = drawable->GetLodGroup();

					const u32 maskDefault    = CRenderer::GenerateSubBucketMask(CRenderer::RB_MODEL_DEFAULT   );
				//	const u32 maskShadow     = CRenderer::GenerateSubBucketMask(CRenderer::RB_MODEL_SHADOW    );
					const u32 maskReflection = CRenderer::GenerateSubBucketMask(CRenderer::RB_MODEL_REFLECTION);
					const u32 maskMirror     = CRenderer::GenerateSubBucketMask(CRenderer::RB_MODEL_MIRROR    );
					const u32 maskWater      = CRenderer::GenerateSubBucketMask(CRenderer::RB_MODEL_WATER     );

					const u32 maskNonShadow  = maskDefault | maskReflection | maskMirror | maskWater;

					for (int lodIndex = 0; lodIndex < LOD_COUNT; lodIndex++)
					{
						if (lodGroup.ContainsLod(lodIndex))
						{
							const rmcLod& lod = lodGroup.GetLod(lodIndex);

							for (int lodModelIndex = 0; lodModelIndex < lod.GetCount(); lodModelIndex++)
							{
								const grmModel* pModel = lod.GetModel(lodModelIndex);

								if (pModel)
								{
									const grmModel& model = *pModel;
									const u32 mask = CRenderer::GetSubBucketMask(model.ComputeBucketMask(*pShaderGroup));

									if (mask & maskNonShadow)
									{
										for (int geomIndex = 0; geomIndex < model.GetGeometryCount(); geomIndex++)
										{
											//const grmGeometry& geom = model.GetGeometry(geomIndex);
											const int shaderIndex = model.GetShaderIndex(geomIndex);

											if (AssertVerify(shaderIndex >= 0 && shaderIndex < pShaderGroup->GetCount()))
											{
												shaderFlags[shaderIndex/8] |= BIT(shaderIndex%8);
											}
										}
									}
								}
							}
						}
					}
				}

				for (int i = 0; i < pShaderGroup->GetCount(); i++)
				{
					if (shaderFlags == NULL || (shaderFlags[i/8] & BIT(i%8)) != 0)
					{
						StreamingIteratorTestMaterial_T(store, slot, entry, &pShaderGroup->GetShader(i), i);
					}
				}

				if (shaderFlags)
				{
					delete[] shaderFlags;
				}
			}
		}

		// ===

		if (g_siTestTextureFlags)
		{
			StreamingIteratorTestTxd_T(store, slot, entry, drawable->GetTexDictSafe());
		}
	}
}

static void StreamingIteratorTestFragmentWindows(const Fragment* fragment, const char* path)
{
#if VEHICLE_GLASS_CONSTRUCTOR
	const fwVehicleGlassWindowData* pWindowData = static_cast<const gtaFragType*>(fragment)->m_vehicleWindowData;

	if (pWindowData)
	{
		const fwVehicleGlassWindowHeader& header = *(const fwVehicleGlassWindowHeader*)pWindowData;
		const fwVehicleGlassWindowRef* windowRefs = (const fwVehicleGlassWindowRef*)((const u8*)pWindowData + sizeof(fwVehicleGlassWindowHeader));

		Assert(header.m_tag == 'VGWH');

		for (int i = 0; i < header.m_numWindows; i++)
		{
			const int componentId = windowRefs[i].m_componentId;
			const fwVehicleGlassWindow* pWindow = fwVehicleGlassWindowData::GetWindow(pWindowData, componentId);

			if (pWindow)
			{
				const u32 flags = pWindow->GetFlags();

				Displayf("%s,%d,0x%08x,%s", path, componentId, flags, pWindow->m_dataRLE ? "" : "NULL"); // NULL data indicates bulletproof or siren glass
			}
		}
	}
#else
	(void)fragment;
	(void)path;
#endif
}

static void StreamingIteratorTestFragment(const Fragment* fragment, const char* path)
{
	if (fragment)
	{
		if (g_siTestFragmentFlags & SI_TEST_FRAGMENT_WINDOWS)
		{
			StreamingIteratorTestFragmentWindows(fragment, path);
		}
	}
}

static bool StreamingIteratorFilter(const char* rpfPath, const char* assetName)
{
	if (g_siFilterRpfPath[0] != '\0' && strstr(rpfPath, g_siFilterRpfPath) == NULL)
	{
		return false;
	}

	if (g_siFilterAssetName[0] != '\0' && strstr(assetName, g_siFilterAssetName) == NULL)
	{
		return false;
	}

	if (g_siFilterSkipFlags & SI_FILTER_FLAG_SKIP_VEHICLES)
	{
		if (stristr(rpfPath, "/vehicle"))
		{
			return false;
		}
	}

	if (g_siFilterSkipFlags & SI_FILTER_FLAG_SKIP_PEDS)
	{
		if (stristr(rpfPath, "/componentped") ||
			stristr(rpfPath, "/streamedped") ||
			stristr(rpfPath, "/pedprops") ||
			stristr(rpfPath, "/ped_") ||
			stristr(rpfPath, "/cutspeds"))
		{
			return false;
		}
	}

	if (g_siFilterSkipFlags & SI_FILTER_FLAG_SKIP_PROPS)
	{
		if (stristr(rpfPath, "/props/"))
		{
			return false;
		}
	}

	if (g_siFilterSkipFlags & SI_FILTER_FLAG_SKIP_INTERIORS)
	{
		if (stristr(rpfPath, "/interiors/"))
		{
			return false;
		}
	}

	if (g_siFilterSkipFlags & SI_FILTER_FLAG_SKIP_Z_Z_ASSETS)
	{
		if (stristr(assetName, "z_z_"))
		{
			return false;
		}
	}

	return true;
}

static bool StreamingIteratorTxdFilterFunc(const fwTxdDef*, const char* rpfPath, const char* assetName, int, void*)
{
	if (!StreamingIteratorFilter(rpfPath, assetName))
	{
		return false;
	}

	return true;
}

static bool StreamingIteratorDwdFilterFunc(const fwDwdDef*, const char* rpfPath, const char* assetName, int, void*)
{
	if (!StreamingIteratorFilter(rpfPath, assetName))
	{
		return false;
	}

	return true;
}

static bool StreamingIteratorDrawableFilterFunc(const fwDrawableDef*, const char* rpfPath, const char* assetName, int, void*)
{
	if (!StreamingIteratorFilter(rpfPath, assetName))
	{
		return false;
	}

	return true;
}

static bool StreamingIteratorFragmentFilterFunc(const fwFragmentDef*, const char* rpfPath, const char* assetName, int, void*)
{
	if (!StreamingIteratorFilter(rpfPath, assetName))
	{
		return false;
	}

	return true;
}

static bool StreamingIteratorParticleFilterFunc(const ptxFxListDef*, const char* rpfPath, const char* assetName, int, void*)
{
	if (!StreamingIteratorFilter(rpfPath, assetName))
	{
		return false;
	}

	return true;
}

static bool StreamingIteratorMapDataFilterFunc(const fwMapDataDef* def, const char* rpfPath, const char* assetName, int, void*)
{
	if (!StreamingIteratorFilter(rpfPath, assetName))
	{
		return false;
	}

	if (def)
	{
		const strLocalIndex  slotIndex   = strLocalIndex(fwMapDataStore::GetStore().GetSlotIndex(def));
		const bool bIsHD       = (def->GetContentFlags() & fwMapData::CONTENTFLAG_ENTITIES_HD) != 0;
		const bool bIsLODLight = (def->GetContentFlags() & (fwMapData::CONTENTFLAG_LOD_LIGHTS | fwMapData::CONTENTFLAG_DISTANT_LOD_LIGHTS)) != 0;
		const bool bIsInterior = def->GetIsMLOInstanceDef();

		if (bIsHD) { if (g_siFilterSkipFlags & SI_FILTER_FLAG_SKIP_MAPDATA_HD)    { return false; } }
		else       { if (g_siFilterSkipFlags & SI_FILTER_FLAG_SKIP_MAPDATA_NONHD) { return false; } }

		if (bIsInterior) { if (g_siFilterSkipFlags & SI_FILTER_FLAG_SKIP_MAPDATA_INTERIOR) { return false; } }
		else             { if (g_siFilterSkipFlags & SI_FILTER_FLAG_SKIP_MAPDATA_EXTERIOR) { return false; } }

		if (def->GetIsScriptManaged() && (g_siFilterSkipFlags & SI_FILTER_FLAG_SKIP_MAPDATA_SCRIPT) != 0)
		{
			return false;
		}

		if (bIsLODLight && (g_siFilterSkipFlags & SI_FILTER_FLAG_SKIP_MAPDATA_LODLIGHT) != 0)
		{
			return false;
		}

		if (!spdAABB(g_siFilterMapDataPhysicalBoundsMin, g_siFilterMapDataPhysicalBoundsMax).IntersectsAABB(fwMapDataStore::GetStore().GetPhysicalBounds(slotIndex)))
		{
			return false;
		}
	}

	return true;
}

// ================================================================================================

static void StreamingIteratorTxdIteratorFunc(const fwTxdDef* def, int slot, int frames, void*)
{
	const fwTxd* txd = g_TxdStore.Get(strLocalIndex(slot));

	if (g_siShowInfo)
	{
		const atString assetPath = AssetAnalysis::GetAssetPath(g_TxdStore, slot);

		if (txd) { Displayf("streamed in %s after %d frames", assetPath.c_str(), frames); }
		else     { Displayf("failed to stream in %s", assetPath.c_str()); }
	}

	if (def)
	{
		AssetAnalysis::ProcessTxdSlot(slot);

		if (g_siTestTextureFlags)
		{
			StreamingIteratorTestTxd_T(g_TxdStore, slot, INDEX_NONE, g_TxdStore.Get(strLocalIndex(slot)));
		}
	}
}

static void StreamingIteratorDwdIteratorFunc(const fwDwdDef* def, int slot, int frames, void*)
{
	const Dwd* dwd = g_DwdStore.Get(strLocalIndex(slot));

	if (g_siShowInfo)
	{
		const atString assetPath = AssetAnalysis::GetAssetPath(g_DwdStore, slot);

		if (dwd) { Displayf("streamed in %s after %d frames", assetPath.c_str(), frames); }
		else     { Displayf("failed to stream in %s", assetPath.c_str()); }
	}

	if (def)
	{
		AssetAnalysis::ProcessDwdSlot(slot);
		GeomCollector::ProcessDwdSlot(slot);

		if (g_siTestDrawableFlags |
			g_siTestMaterialFlags |
			g_siTestTextureFlags)
		{
			if (dwd)
			{
				for (int i = 0; i < dwd->GetCount(); i++)
				{
					StreamingIteratorTestDrawable_T(g_DwdStore, slot, i, dwd->GetEntry(i));
				}
			}
		}
	}
}

static void StreamingIteratorDrawableIteratorFunc(const fwDrawableDef* def, int slot, int frames, void*)
{
	const Drawable* drawable = g_DrawableStore.Get(strLocalIndex(slot));

	if (g_siShowInfo)
	{
		const atString assetPath = AssetAnalysis::GetAssetPath(g_DrawableStore, slot);

		if (drawable) { Displayf("streamed in %s after %d frames", assetPath.c_str(), frames); }
		else          { Displayf("failed to stream in %s", assetPath.c_str()); }
	}

	if (def)
	{
		AssetAnalysis::ProcessDrawableSlot(slot);
		GeomCollector::ProcessDrawableSlot(slot);

		if (g_siTestDrawableFlags |
			g_siTestMaterialFlags |
			g_siTestTextureFlags)
		{
			StreamingIteratorTestDrawable_T(g_DrawableStore, slot, INDEX_NONE, g_DrawableStore.Get(strLocalIndex(slot)));
		}
	}
}

static void StreamingIteratorFragmentIteratorFunc(const fwFragmentDef* def, int slot, int frames, void*)
{
	const Fragment* fragment = g_FragmentStore.Get(strLocalIndex(slot));

	if (g_siShowInfo)
	{
		const atString assetPath = AssetAnalysis::GetAssetPath(g_FragmentStore, slot);

		if (fragment) { Displayf("streamed in %s after %d frames", assetPath.c_str(), frames); }
		else          { Displayf("failed to stream in %s", assetPath.c_str()); }
	}

	if (def)
	{
		AssetAnalysis::ProcessFragmentSlot(slot);
		GeomCollector::ProcessFragmentSlot(slot);

		if (g_siTestDrawableFlags |
			g_siTestMaterialFlags |
			g_siTestTextureFlags)
		{
			if (fragment)
			{
				StreamingIteratorTestDrawable_T(g_FragmentStore, slot, AssetAnalysis::FRAGMENT_COMMON_DRAWABLE_ENTRY, fragment->GetCommonDrawable());
				StreamingIteratorTestDrawable_T(g_FragmentStore, slot, AssetAnalysis::FRAGMENT_CLOTH_DRAWABLE_ENTRY, fragment->GetClothDrawable());

				for (int i = 0; i < fragment->GetNumExtraDrawables(); i++)
				{
					StreamingIteratorTestDrawable_T(g_FragmentStore, slot, i, fragment->GetExtraDrawable(i));
				}
			}
		}

		if (g_siTestFragmentFlags)
		{
			StreamingIteratorTestFragment(g_FragmentStore.Get(strLocalIndex(slot)), AssetAnalysis::GetAssetPath(g_FragmentStore, slot, INDEX_NONE).c_str());
		}
	}
}

static void StreamingIteratorParticleIteratorFunc(const ptxFxListDef* def, int slot, int frames, void*)
{
	const ptxFxList* particle = g_ParticleStore.Get(strLocalIndex(slot));

	if (g_siShowInfo)
	{
		const atString assetPath = AssetAnalysis::GetAssetPath(g_ParticleStore, slot);

		if (particle) { Displayf("streamed in %s after %d frames", assetPath.c_str(), frames); }
		else          { Displayf("failed to stream in %s", assetPath.c_str()); }
	}

	if (def)
	{
		AssetAnalysis::ProcessParticleSlot(slot);
	}
}

static void StreamingIteratorMapDataIteratorFunc(const fwMapDataDef* def, int slot, int frames, void*)
{
	const fwMapDataContents* mapData = def ? def->m_pObject : NULL;//g_MapDataStore.Get(slot);

	if (g_siShowInfo)
	{
		const atString assetPath = AssetAnalysis::GetAssetPath(g_MapDataStore, slot);

		if (mapData) { Displayf("streamed in %s after %d frames", assetPath.c_str(), frames); }
		else         { Displayf("failed to stream in %s after %d frames", assetPath.c_str(), frames); }
	}

	if (def)
	{
		AssetAnalysis::ProcessMapDataSlot(slot);
		GeomCollector::ProcessMapDataSlot(slot);

		if (g_siTestSkipShadowProxies)
		{
			if (mapData)
			{
				for (int i = 0; i < mapData->GetNumEntities(); i++)
				{
					const CEntity* entity = static_cast<const CEntity*>(mapData->GetEntities()[i]);

					if (entity)
					{
						const CBaseModelInfo* modelInfo = entity->GetBaseModelInfo();

						if (modelInfo)
						{
							if (entity->GetRenderPhaseVisibilityMask().IsFlagSet(VIS_PHASE_MASK_SHADOWS) && !entity->GetRenderPhaseVisibilityMask().IsFlagSet(VIS_PHASE_MASK_GBUF | VIS_PHASE_MASK_PARABOLOID_REFLECTION | VIS_PHASE_MASK_WATER_REFLECTION | VIS_PHASE_MASK_MIRROR_REFLECTION))
							{
								Displayf("%s is shadow-only", modelInfo->GetModelName());
								g_siTestSkipDrawableNames[modelInfo->GetModelNameHash()] = true;
							}

							if (modelInfo->GetModelType() == MI_TYPE_MLO)
							{
								const atArray<fwEntityDef*>* interiorEntityDefs = &static_cast<CMloModelInfo*>(const_cast<CBaseModelInfo*>(modelInfo))->GetEntities();

								if (interiorEntityDefs) // this can be NULL?
								{
									for (int j = 0; j < interiorEntityDefs->GetCount(); j++)
									{
										const fwEntityDef* interiorEntityDef = interiorEntityDefs->operator[](j);

										if (interiorEntityDef && (interiorEntityDef->m_flags & fwEntityDef::FLAG_ONLY_RENDER_IN_SHADOWS) != 0)
										{
											const u32 interiorModelIndex = CModelInfo::GetModelIdFromName(interiorEntityDef->m_archetypeName).GetModelIndex();

											if (interiorModelIndex != fwModelId::MI_INVALID)
											{
												const CBaseModelInfo* interiorModelInfo = CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(interiorModelIndex)));

												if (interiorModelInfo)
												{
													Displayf("%s is shadow-only", interiorModelInfo->GetModelName());
													g_siTestSkipDrawableNames[interiorModelInfo->GetModelNameHash()] = true;
												}
											}
										}
									}
								}
							}
						}
					}
				}
			}			
		}

		if (g_siTestDrawableFlags & SI_TEST_DRAWABLE_HD_WITH_SUBMESHES)
		{
			if (mapData)
			{
				for (int i = 0; i < mapData->GetNumEntities(); i++)
				{
					const CEntity* entity = static_cast<const CEntity*>(mapData->GetEntities()[i]);

					if (entity)
					{
						if (entity->GetLodData().GetLodType() == LODTYPES_DEPTH_HD ||
							entity->GetLodData().GetLodType() == LODTYPES_DEPTH_ORPHANHD)
						{
							const CBaseModelInfo* modelInfo = entity->GetBaseModelInfo();

							if (modelInfo)
							{
								if (modelInfo->GetDrawableType() == fwArchetype::DT_DRAWABLE ||
									modelInfo->GetDrawableType() == fwArchetype::DT_FRAGMENT)
								{
									g_siTestHDModels[modelInfo->GetModelNameHash()] = true;
								}

								if (modelInfo->GetModelType() == MI_TYPE_MLO)
								{
									const atArray<fwEntityDef*>* interiorEntityDefs = &static_cast<CMloModelInfo*>(const_cast<CBaseModelInfo*>(modelInfo))->GetEntities();

									if (interiorEntityDefs) // this can be NULL?
									{
										for (int j = 0; j < interiorEntityDefs->GetCount(); j++)
										{
											const fwEntityDef* interiorEntityDef = interiorEntityDefs->operator[](j);

											if (interiorEntityDef)
											{
												if (interiorEntityDef->m_lodLevel == LODTYPES_DEPTH_HD ||
													interiorEntityDef->m_lodLevel == LODTYPES_DEPTH_ORPHANHD)
												{
													const u32 interiorModelIndex = CModelInfo::GetModelIdFromName(interiorEntityDef->m_archetypeName).GetModelIndex();

													if (interiorModelIndex != fwModelId::MI_INVALID)
													{
														const CBaseModelInfo* interiorModelInfo = CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(interiorModelIndex)));

														if (interiorModelInfo)
														{
															if (interiorModelInfo->GetDrawableType() == fwArchetype::DT_DRAWABLE ||
																interiorModelInfo->GetDrawableType() == fwArchetype::DT_FRAGMENT)
															{
																g_siTestHDModels[interiorModelInfo->GetModelNameHash()] = true;
															}
														}
													}
												}
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}

		// TODO -- should we call g_MapDataStore.SafeRemove(slot) now? (what if it's already in use by the scene?)
	}
}

// ================================================================================================

static atString GetSimpleTimeString()
{
	const float tsecs = (float)(fwTimer::GetSystemTimeInMilliseconds() - g_siTimeStarted)/1000.0f;
#if 0
	const float hrs  = floorf(tsecs/(60.0f*60.0f));
	const float mins = floorf(tsecs/60.0f - hrs*60.0f);
	const float secs = ceilf(tsecs - (hrs*60.0f + mins)*60.0f);

	if (hrs > 0.0f)
	{
		return atVarString("%d:%02d'%02d\"", (int)hrs, (int)mins, (int)secs);
	}
	else
	{
		return atVarString("%02d'%02d\"", (int)mins, (int)secs);
	}
#else
	if      (tsecs/60.0f >= 100.0f) { return atVarString("%.2f hrs", tsecs/(60.0f*60.0f)); }
	else if (tsecs       >= 100.0f) { return atVarString("%.2f mins", tsecs/60.0f); }
	else                            { return atVarString("%.2f secs", tsecs); }
#endif
}

template <typename T> static void StreamingIteratorCompleteFunc(const char* name, T& store, const atArray<CStreamingIteratorSlot>& failedSlots)
{
	Displayf("%s iterator complete (@ +%s)", name, GetSimpleTimeString().c_str());

	for (int i = 0; i < failedSlots.GetCount(); i++)
	{
		Displayf("failed to stream in %s after %d frames", AssetAnalysis::GetAssetPath(store, failedSlots[i].m_slot.Get()).c_str(), failedSlots[i].m_frames);
	}
}

static void StreamingIteratorTxdCompleteFunc(const atArray<CStreamingIteratorSlot>& failedSlots, void*)
{
	StreamingIteratorCompleteFunc("txd", g_TxdStore, failedSlots);
}

static void StreamingIteratorDwdCompleteFunc(const atArray<CStreamingIteratorSlot>& failedSlots, void*)
{
	StreamingIteratorCompleteFunc("dwd", g_DwdStore, failedSlots);
}

static void StreamingIteratorDrawableCompleteFunc(const atArray<CStreamingIteratorSlot>& failedSlots, void*)
{
	StreamingIteratorCompleteFunc("drawable", g_DrawableStore, failedSlots);
}

static void StreamingIteratorFragmentCompleteFunc(const atArray<CStreamingIteratorSlot>& failedSlots, void*)
{
	StreamingIteratorCompleteFunc("fragment", g_FragmentStore, failedSlots);
}

static void StreamingIteratorParticleCompleteFunc(const atArray<CStreamingIteratorSlot>& failedSlots, void*)
{
	StreamingIteratorCompleteFunc("particle", g_ParticleStore, failedSlots);
}

static void StreamingIteratorMapDataCompleteFunc(const atArray<CStreamingIteratorSlot>& failedSlots, void*)
{
	StreamingIteratorCompleteFunc("mapdata", g_MapDataStore, failedSlots);
}

namespace rage
{
	XPARAM(nopopups);
}

static void TeleportToTimbuktu_button()
{
	const Vec3V timbuktu(0.0f, 0.0f, 9000.0f);
	CPed* pPlayerPed = FindPlayerPed();

	if (pPlayerPed)
	{
		pPlayerPed->Teleport(GTA_ONLY(RCC_VECTOR3)(timbuktu), 0.0f);
	}

	const Vec3V camFront = Vec3V(V_Z_AXIS_WZERO); // looking up to sky
	const Vec3V camUp    = Vec3V(V_Y_AXIS_WZERO);
	const Vec3V camLeft  = Cross(camFront, camUp);
	const Vec3V camPos   = timbuktu;

	const Mat34V camMtx(camLeft, camFront, camUp, camPos);

	camInterface::GetDebugDirector().ActivateFreeCam();
	camInterface::GetDebugDirector().GetFreeCamFrameNonConst().SetWorldMatrix(RCC_MATRIX34(camMtx), false); // set both orientation and position

	fwTimer::StartUserPause();
}

__COMMENT(static) void CStreamingIteratorManager::AddWidgets(bkBank& bk)
{
	//bk.PushGroup("Streaming Iterator", false);
	{
		class StartAssetAnalysis_button { public: static void func()
		{
			AssetAnalysis::Start();

			g_TxdIterator     .GetEnabledRef() = true;
			g_DwdIterator     .GetEnabledRef() = true;
			g_DrawableIterator.GetEnabledRef() = true;
			g_FragmentIterator.GetEnabledRef() = true;
			g_ParticleIterator.GetEnabledRef() = true;
			g_MapDataIterator .GetEnabledRef() = true;

			g_siBreakWhenFinished = true;

			g_siEnabled = true;
		}};

		class StartGeomCollector_button { public: static void func()
		{
			GeomCollector::Start();

		//	g_TxdIterator     .GetEnabledRef() = true;
			g_DwdIterator     .GetEnabledRef() = true;
			g_DrawableIterator.GetEnabledRef() = true;
			g_FragmentIterator.GetEnabledRef() = true;
		//	g_ParticleIterator.GetEnabledRef() = true;
		//	g_MapDataIterator .GetEnabledRef() = true;

			g_siQueueMapDataIterator = true;
			g_siBreakWhenFinished = true;

			g_siEnabled = true;
		}};

		class StartAll_button { public: static void func()
		{
			AssetAnalysis::Start();
			GeomCollector::Start();

			g_TxdIterator     .GetEnabledRef() = true;
			g_DwdIterator     .GetEnabledRef() = true;
			g_DrawableIterator.GetEnabledRef() = true;
			g_FragmentIterator.GetEnabledRef() = true;
			g_ParticleIterator.GetEnabledRef() = true;
		//	g_MapDataIterator .GetEnabledRef() = true;

			g_siQueueMapDataIterator = true;
			g_siBreakWhenFinished = true;

			g_siEnabled = true;
		}};

		static bool s_popups = true;
		s_popups = !PARAM_nopopups.Get();

		class Popups_update { public: static void func()
		{
			Displayf("popups %s", s_popups ? "enabled" : "disabled");
			PARAM_nopopups.Set(s_popups ? NULL : "");
		}};

		bk.AddButton("Teleport to Timbuktu"     , TeleportToTimbuktu_button);
		bk.AddButton("Start Asset Analysis"     , StartAssetAnalysis_button::func);
		bk.AddButton("Start Geom Collector"     , StartGeomCollector_button::func);
		bk.AddButton("Start All"                , StartAll_button::func);
		bk.AddToggle("Popups"                   , &s_popups, Popups_update::func);
		bk.AddToggle("Enabled"                  , &g_siEnabled);
		bk.AddSeparator();
		bk.AddToggle("Enabled - Txd"            , &g_TxdIterator     .GetEnabledRef());
		bk.AddToggle("Enabled - Dwd"            , &g_DwdIterator     .GetEnabledRef());
		bk.AddToggle("Enabled - Drawable"       , &g_DrawableIterator.GetEnabledRef());
		bk.AddToggle("Enabled - Fragment"       , &g_FragmentIterator.GetEnabledRef());
		bk.AddToggle("Enabled - Particle"       , &g_ParticleIterator.GetEnabledRef());
		bk.AddToggle("Enabled - MapData"        , &g_MapDataIterator .GetEnabledRef());
		bk.AddSeparator();
		bk.AddToggle("Queue MapData Iterator"   , &g_siQueueMapDataIterator);
		bk.AddToggle("Break When Finished"      , &g_siBreakWhenFinished);
		bk.AddToggle("Show Info"                , &g_siShowInfo);
		AssetAnalysis::AddWidgets(bk);
		GeomCollector::AddWidgets(bk);
		bk.AddButton("Finish and Reset"         , FinishAndReset);

		bk.AddSeparator();

		bk.AddSlider("Slot - Txd"     , &g_TxdIterator     .GetCurrentSlotRef(), 0, g_TxdStore     .GetCount(), 1);
		bk.AddSlider("Slot - Dwd"     , &g_DwdIterator     .GetCurrentSlotRef(), 0, g_DwdStore     .GetCount(), 1);
		bk.AddSlider("Slot - Drawable", &g_DrawableIterator.GetCurrentSlotRef(), 0, g_DrawableStore.GetCount(), 1);
		bk.AddSlider("Slot - Fragment", &g_FragmentIterator.GetCurrentSlotRef(), 0, g_FragmentStore.GetCount(), 1);
		bk.AddSlider("Slot - Particle", &g_ParticleIterator.GetCurrentSlotRef(), 0, g_ParticleStore.GetCount(), 1);
		bk.AddSlider("Slot - MapData" , &g_MapDataIterator .GetCurrentSlotRef(), 0, g_MapDataStore .GetCount(), 1);

		bk.AddSeparator();

		bk.AddText  ("Search RPF Path"        , &g_siFilterRpfPath[0], sizeof(g_siFilterRpfPath), false);
		bk.AddText  ("Search Asset Name"      , &g_siFilterAssetName[0], sizeof(g_siFilterAssetName), false);
		bk.AddToggle("Skip Vehicles"          , &g_siFilterSkipFlags, SI_FILTER_FLAG_SKIP_VEHICLES);
		bk.AddToggle("Skip Peds"              , &g_siFilterSkipFlags, SI_FILTER_FLAG_SKIP_PEDS);
		bk.AddToggle("Skip Props"             , &g_siFilterSkipFlags, SI_FILTER_FLAG_SKIP_PROPS);
		bk.AddToggle("Skip Interiors"         , &g_siFilterSkipFlags, SI_FILTER_FLAG_SKIP_INTERIORS);
		bk.AddToggle("Skip MapData - HD"      , &g_siFilterSkipFlags, SI_FILTER_FLAG_SKIP_MAPDATA_HD);
		bk.AddToggle("Skip MapData - Non-HD"  , &g_siFilterSkipFlags, SI_FILTER_FLAG_SKIP_MAPDATA_NONHD);
		bk.AddToggle("Skip MapData - Interior", &g_siFilterSkipFlags, SI_FILTER_FLAG_SKIP_MAPDATA_INTERIOR);
		bk.AddToggle("Skip MapData - Exterior", &g_siFilterSkipFlags, SI_FILTER_FLAG_SKIP_MAPDATA_EXTERIOR);
		bk.AddToggle("Skip MapData - Script"  , &g_siFilterSkipFlags, SI_FILTER_FLAG_SKIP_MAPDATA_SCRIPT);
		bk.AddToggle("Skip MapData - LODLight", &g_siFilterSkipFlags, SI_FILTER_FLAG_SKIP_MAPDATA_LODLIGHT);
		bk.AddToggle("Skip Z_Z_ Assets"       , &g_siFilterSkipFlags, SI_FILTER_FLAG_SKIP_Z_Z_ASSETS);

		bk.AddSeparator();

		bk.AddVector("MapData Min", &g_siFilterMapDataPhysicalBoundsMin, -16000.0f, 16000.0f, 1.0f);
		bk.AddVector("MapData Max", &g_siFilterMapDataPhysicalBoundsMax, -16000.0f, 16000.0f, 1.0f);

		bk.AddSeparator();

		bk.AddToggle("Test Skip Shadow Proxies"       , &g_siTestSkipShadowProxies);
		bk.AddToggle("Test Drawable Num Tris"         , &g_siTestDrawableFlags, SI_TEST_DRAWABLE_NUM_TRIS);
		bk.AddToggle("Test Drawable Orientation"      , &g_siTestDrawableFlags, SI_TEST_DRAWABLE_ORIENTATION);
		bk.AddToggle("Test Drawable LODs"             , &g_siTestDrawableFlags, SI_TEST_DRAWABLE_LODS);
		bk.AddToggle("Test Drawable Water Flow"       , &g_siTestDrawableFlags, SI_TEST_DRAWABLE_WATER_FLOW);
		bk.AddToggle("Test Drawable Cable Shader Vars", &g_siTestDrawableFlags, SI_TEST_DRAWABLE_CABLE_SHADER_VARS);
		bk.AddToggle("Test Drawable Cable Normals"    , &g_siTestDrawableFlags, SI_TEST_DRAWABLE_CABLE_NORMALS);
		bk.AddToggle("Test Drawable HD With Submeshes", &g_siTestDrawableFlags, SI_TEST_DRAWABLE_HD_WITH_SUBMESHES);
		bk.AddToggle("Test Drawable Multiple Buckets" , &g_siTestDrawableFlags, SI_TEST_DRAWABLE_MULTIPLE_BUCKETS);
		bk.AddToggle("Test Fragment Windows"          , &g_siTestFragmentFlags, SI_TEST_FRAGMENT_WINDOWS);
		bk.AddToggle("Test Material Low Fresnel"      , &g_siTestMaterialFlags, SI_TEST_MATERIAL_LOW_FRESNEL);
		bk.AddToggle("Test Material No Diffuse Tex"   , &g_siTestMaterialFlags, SI_TEST_MATERIAL_NO_DIFFUSE_TEX);
		bk.AddToggle("Test Material Single Param Tint", &g_siTestMaterialFlags, SI_TEST_MATERIAL_SINGLE_PARAM_TINT);
		bk.AddToggle("Test Texture Size"              , &g_siTestTextureFlags, SI_TEST_TEXTURE_SIZE);
		bk.AddSlider(" - min size"                    , &g_siTestTextureMinSize, 0, 8192, 1);
		bk.AddSlider(" - max size"                    , &g_siTestTextureMaxSize, 0, 8192, 1);
		bk.AddToggle("Test Texture Mip Size"          , &g_siTestTextureFlags, SI_TEST_TEXTURE_MIP_SIZE);
		bk.AddToggle("Test Texture Non Pow2"          , &g_siTestTextureFlags, SI_TEST_TEXTURE_NON_POW2);
		bk.AddToggle("Test Texture Paths"             , &g_siTestTextureFlags, SI_TEST_TEXTURE_PATHS);
		bk.AddText  ("Test Texture Name Filter"       , &g_siTestTextureNameFilter[0], sizeof(g_siTestTextureNameFilter), false);
		bk.AddText  ("Test CSV Output Path"           , &g_siTestCSVOutputPath[0], sizeof(g_siTestCSVOutputPath), false);

		g_TxdIterator     .SetFilterFunc(StreamingIteratorTxdFilterFunc);
		g_DwdIterator     .SetFilterFunc(StreamingIteratorDwdFilterFunc);
		g_DrawableIterator.SetFilterFunc(StreamingIteratorDrawableFilterFunc);
		g_FragmentIterator.SetFilterFunc(StreamingIteratorFragmentFilterFunc);
		g_ParticleIterator.SetFilterFunc(StreamingIteratorParticleFilterFunc);
		g_MapDataIterator .SetFilterFunc(StreamingIteratorMapDataFilterFunc);

		g_TxdIterator     .SetIteratorFunc(StreamingIteratorTxdIteratorFunc);
		g_DwdIterator     .SetIteratorFunc(StreamingIteratorDwdIteratorFunc);
		g_DrawableIterator.SetIteratorFunc(StreamingIteratorDrawableIteratorFunc);
		g_FragmentIterator.SetIteratorFunc(StreamingIteratorFragmentIteratorFunc);
		g_ParticleIterator.SetIteratorFunc(StreamingIteratorParticleIteratorFunc);
		g_MapDataIterator .SetIteratorFunc(StreamingIteratorMapDataIteratorFunc);

		g_TxdIterator     .SetCompleteFunc(StreamingIteratorTxdCompleteFunc);
		g_DwdIterator     .SetCompleteFunc(StreamingIteratorDwdCompleteFunc);
		g_DrawableIterator.SetCompleteFunc(StreamingIteratorDrawableCompleteFunc);
		g_FragmentIterator.SetCompleteFunc(StreamingIteratorFragmentCompleteFunc);
		g_ParticleIterator.SetCompleteFunc(StreamingIteratorParticleCompleteFunc);
		g_MapDataIterator .SetCompleteFunc(StreamingIteratorMapDataCompleteFunc);

#if EFFECT_PRESERVE_STRINGS
		bk.PushGroup("Scene Tests", false);
		{
			static char s_shaderName[128] = "dirt_normal_tnt";
			static char s_shaderVarName[128] = "SingleParamTint";

			class FindShaderVars_button { public: static void func()
			{
				class FindShaderVarsInEntity { public: static bool func(void* item, void*)
				{
					const CEntity* pEntity = static_cast<const CEntity*>(item);

					if (pEntity)
					{
						const Drawable* pDrawable = pEntity->GetDrawable();

						if (pDrawable)
						{
							const grmShaderGroup* pShaderGroup = pDrawable->GetShaderGroupPtr();

							if (pShaderGroup)
							{
								for (int shaderIndex = 0; shaderIndex < pShaderGroup->GetCount(); shaderIndex++)
								{
									const grmShader& shader = pShaderGroup->GetShader(shaderIndex);

									if (s_shaderName[0] == '\0' || strcmp(shader.GetName(), s_shaderName) == 0)
									{
										const grcEffectVar idVar = shader.LookupVar(s_shaderVarName);
										char valueStr[128] = "UNDEFINED";

										if (idVar)
										{
											const char* varName = "";
											grcEffect::VarType varType = grcEffect::VT_NONE;
											int varAnnotationCount = 0;
											bool varIsGlobal = false;
											shader.GetVarDesc(idVar, varName, varType, varAnnotationCount, varIsGlobal, NULL);

											if (varType == grcEffect::VT_TEXTURE)
											{
												grcTexture* texture = NULL;
												shader.GetVar(idVar, texture);
												sprintf(valueStr, "%s", texture ? texture->GetName() : "NULL");
											}
											else if (varType == grcEffect::VT_FLOAT)
											{
												float value = 0.0f;
												shader.GetVar(idVar, value);
												sprintf(valueStr, "%f", value);
											}
											else if (varType == grcEffect::VT_VECTOR2)
											{
												Vector2 value(0.0f, 0.0f);
												shader.GetVar(idVar, value);
												sprintf(valueStr, "%f,%f", value.x, value.y);
											}
											else if (varType == grcEffect::VT_VECTOR3)
											{
												Vec3V value(V_ZERO);
												shader.GetVar(idVar, RC_VECTOR3(value));
												sprintf(valueStr, "%f,%f,%f", VEC3V_ARGS(value));
											}
											else if (varType == grcEffect::VT_VECTOR4)
											{
												Vec4V value(V_ZERO);
												shader.GetVar(idVar, RC_VECTOR4(value));
												sprintf(valueStr, "%f,%f,%f,%f", VEC4V_ARGS(value));
											}
											else
											{
												sprintf(valueStr, "(unhandled var type %d)", varType);
											}
										}

										Displayf("%s @ %f,%f,%f uses %s (shaderIndex=%d) .. %s = %s",
											pEntity->GetModelName(),
											VEC3V_ARGS(pEntity->GetTransform().GetPosition()),
											shader.GetName(),
											shaderIndex,
											s_shaderVarName,
											valueStr
										);
									}
								}
							}
						}
					}

					return true;
				}};

				CBuilding        ::GetPool()->ForAll(FindShaderVarsInEntity::func, NULL);
				CAnimatedBuilding::GetPool()->ForAll(FindShaderVarsInEntity::func, NULL);
				CObject          ::GetPool()->ForAll(FindShaderVarsInEntity::func, NULL);
				CDummyObject     ::GetPool()->ForAll(FindShaderVarsInEntity::func, NULL);
			}};

			bk.AddText("Shader Name", &s_shaderName[0], sizeof(s_shaderName), false);
			bk.AddText("Shader Var Name", &s_shaderVarName[0], sizeof(s_shaderVarName), false);
			bk.AddButton("Find Shader Vars", FindShaderVars_button::func);
		}
		bk.PopGroup();
#endif // EFFECT_PRESERVE_STRINGS
	}
	//bk.PopGroup();
}

__COMMENT(static) void CStreamingIteratorManager::Update()
{
	if (g_siEnabled)
	{
		fwMapDataStore::GetStore().GetBoxStreamer().OverrideRequiredAssets(0);

		/*static bool bOnce = true;
		if (bOnce)
		{
			const int bitsSize = (g_MapDataStore.GetSize() + 7)/8;
			u8* bits = rage_new u8[bitsSize];
			sysMemSet(bits, 0, bitsSize);
			g_MapDataIterator.SetSlotsStreamedAndProcessedBits(bits);
			bOnce = false;
			fwMapDataStore::SetHasMapDataSlotBeenStreamedAndProcessed(bits);
		}*/

		bool bAnyStarted = false;

		if (g_TxdIterator     .GetCurrentSlotRef() == 0 && g_TxdIterator     .GetEnabledRef()) { bAnyStarted = true; Displayf("starting %s iterator", "txd"); }
		if (g_DwdIterator     .GetCurrentSlotRef() == 0 && g_DwdIterator     .GetEnabledRef()) { bAnyStarted = true; Displayf("starting %s iterator", "dwd"); }
		if (g_DrawableIterator.GetCurrentSlotRef() == 0 && g_DrawableIterator.GetEnabledRef()) { bAnyStarted = true; Displayf("starting %s iterator", "drawable"); }
		if (g_FragmentIterator.GetCurrentSlotRef() == 0 && g_FragmentIterator.GetEnabledRef()) { bAnyStarted = true; Displayf("starting %s iterator", "fragment"); }
		if (g_ParticleIterator.GetCurrentSlotRef() == 0 && g_ParticleIterator.GetEnabledRef()) { bAnyStarted = true; Displayf("starting %s iterator", "particle"); }
		if (g_MapDataIterator .GetCurrentSlotRef() == 0 && g_MapDataIterator .GetEnabledRef()) { bAnyStarted = true; Displayf("starting %s iterator", "mapdata"); }

		if (bAnyStarted)
		{
			g_siTimeStarted = fwTimer::GetSystemTimeInMilliseconds();
			Displayf("streaming iterators started at systime=%d", g_siTimeStarted);
		}

		const eStreamingIteratorState states[] =
		{
			g_TxdIterator     .Update(),
			g_DwdIterator     .Update(),
			g_DrawableIterator.Update(),
			g_FragmentIterator.Update(),
			g_ParticleIterator.Update(),
			g_MapDataIterator .Update(),
		};

		bool bWasRunning = false;
		bool bNotFinished = false;

		for (int i = 0; i < NELEM(states); i++)
		{
			if (states[i] != STREAMING_ITERATOR_OFF)
			{
				bWasRunning = true;
			}

			if (states[i] == STREAMING_ITERATOR_RUNNING)
			{
				bNotFinished = true;
			}
			else if (states[i] == STREAMING_ITERATOR_COMPLETE)
			{
				const char* names[] = {"txd", "dwd", "drawable", "fragment", "particle", "mapdata"};	
				Displayf("finished %s iterator (@ +%s)", names[i], GetSimpleTimeString().c_str());
			}
		}

		if (bWasRunning && !bNotFinished)
		{
			if (g_siQueueMapDataIterator)
			{
				Displayf("starting mapdata iterator (@ +%s)", GetSimpleTimeString().c_str());
				g_MapDataIterator.GetEnabledRef() = true;
				g_siQueueMapDataIterator = false;
			}
			else
			{
				FinishAndReset();
			}
		}
	}
}

__COMMENT(static) void CStreamingIteratorManager::FinishAndReset()
{
	Displayf("streaming iterators finished (@ +%s)", GetSimpleTimeString().c_str());

	AssetAnalysis::FinishAndReset();
	GeomCollector::FinishAndReset();

	g_siEnabled              = false;
	g_siQueueMapDataIterator = false;
	g_siTimeStarted          = 0;
	g_siTestDrawableNumTris  = 0;

	if (g_siTestCSV)
	{
		g_siTestCSV->Close();
		g_siTestCSV = NULL;
	}

	g_TxdIterator     .Reset();
	g_DwdIterator     .Reset();
	g_DrawableIterator.Reset();
	g_FragmentIterator.Reset();
	g_ParticleIterator.Reset();
	g_MapDataIterator .Reset();

	if (g_siLog)
	{
		g_siLog->Close();
		g_siLog = NULL;
	}

	if (g_siBreakWhenFinished)
	{
		__debugbreak();
		g_siBreakWhenFinished = false;
	}
}

__COMMENT(static) void CStreamingIteratorManager::Logf(const char* format, ...)
{
	if (g_siLog == NULL)
	{
		g_siLog = fiStream::Create("assets:/non_final/si.log");
	}

	if (g_siLog)
	{
		char temp[4096] = "";
		va_list args;
		va_start(args, format);
		vsnprintf(temp, sizeof(temp), format, args);
		va_end(args);

		fprintf(g_siLog, "%s\n", temp);
		g_siLog->Flush();

		Displayf("%s", temp);
	}
}

__COMMENT(static) const atArray<CStreamingIteratorSlot>* CStreamingIteratorManager::GetFailedSlots(int assetType)
{
	switch (assetType)
	{
	case AssetAnalysis::ASSET_TYPE_TXD     : return g_TxdIterator     .GetFailedSlots();
	case AssetAnalysis::ASSET_TYPE_DWD     : return g_DwdIterator     .GetFailedSlots();
	case AssetAnalysis::ASSET_TYPE_DRAWABLE: return g_DrawableIterator.GetFailedSlots();
	case AssetAnalysis::ASSET_TYPE_FRAGMENT: return g_FragmentIterator.GetFailedSlots();
	case AssetAnalysis::ASSET_TYPE_PARTICLE: return g_ParticleIterator.GetFailedSlots();
	case AssetAnalysis::ASSET_TYPE_MAPDATA : return g_MapDataIterator .GetFailedSlots();
	}

	return NULL;
}

#if RDR_VERSION
__COMMENT(static) void CStreamingIteratorManager::StartAreaCodeBoundsCollection()
{
	TeleportToTimbuktu_button();
	GeomCollector::StartAreaCodeBoundsCollection();
	strcpy(g_siFilterRpfPath, "/area/area_");
	g_siFilterSkipFlags = SI_FILTER_FLAG_SKIP_MAPDATA_NONHD | SI_FILTER_FLAG_SKIP_MAPDATA_INTERIOR | SI_FILTER_FLAG_SKIP_MAPDATA_SCRIPT | SI_FILTER_FLAG_SKIP_MAPDATA_LODLIGHT | SI_FILTER_FLAG_SKIP_Z_Z_ASSETS;
	g_MapDataIterator.GetEnabledRef() = true;
	g_siEnabled = true;
}
#endif // RDR_VERSION

#endif // __BANK
