///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	VfxFogVolumeInfo.cpp
//	BY	: 	Mark Nicholson
//	FOR	:	Rockstar North
//	ON	:	25 February 2014
//	WHAT:	
//
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////

#include "VfxFogVolumeInfo.h"
#include "VfxFogVolumeInfo_parser.h"
#include "vfx/VfxHelper.h"
#include "timecycle/TimeCycle.h"

// rage
#include "parser/manager.h"
#include "parser/restparserservices.h"

// framework
#include "fwscene/stores/psostore.h"
#include "fwsys/fileexts.h"
#include "vfx/channel.h"
#include "vfx/vfxwidget.h"

// game
#include "Camera/CamInterface.h"
#include "Scene/DataFileMgr.h"
#include "Renderer/Water.h"


///////////////////////////////////////////////////////////////////////////////
//  OPTIMISATIONS - TURN ON IN OPTIMISATIONS.H
///////////////////////////////////////////////////////////////////////////////

VFX_SYSTEM_OPTIMISATIONS()


///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////

#define	FOGVOLUME_MAX_ITEMS		128


///////////////////////////////////////////////////////////////////////////////
//  GLOBAL VARIABLES
///////////////////////////////////////////////////////////////////////////////

CVfxFogVolumeInfoMgr g_vfxFogVolumeInfoMgr;

#if RSG_BANK
CVfxFogVolumeInfo ms_DebugRegisteredFogVolumeOverrideSettings;
int			ms_DebugRenderRegisteredIndex = -1;
bool		ms_DebugIsolateCurrentRegisteredVolume = false;
bool		ms_DebugRenderRegisteredIndexChanged = false;
bool		ms_DebugRenderRegisteredSettingsChanged = false;
#endif

///////////////////////////////////////////////////////////////////////////////
//  CODE
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  CLASS CVfxFogVolumeInfoMgr
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  LoadData
///////////////////////////////////////////////////////////////////////////////

void CVfxFogVolumeInfoMgr::LoadData()
{
	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::VFXFOGVOLUMEINFO_FILE);
	vfxAssertf(DATAFILEMGR.IsValid(pData), "vfx fog volume info file is not available");
	while (DATAFILEMGR.IsValid(pData))
	{
		fwPsoStoreLoader::LoadDataIntoObject(pData->m_filename, META_FILE_EXT, *this);
		//PARSER.SaveObject(pData->m_filename, "meta", this);
		parRestRegisterSingleton("Vfx/FogVolumeInfo", *this, pData->m_filename);

		pData = DATAFILEMGR.GetNextFile(pData);
	}

#if __BANK
	if(ms_DebugRenderRegisteredIndex >=0 )
	{
		ms_DebugRenderRegisteredIndexChanged = true;
	}
#endif
}


///////////////////////////////////////////////////////////////////////////////
//  FogVolumeItemSorter
///////////////////////////////////////////////////////////////////////////////

int CVfxFogVolumeInfoMgr::FogVolumeItemSorter(const void* pA, const void* pB)
{
	// get the events
	const CVfxFogVolumeInfo& fogVolumeInfoA = **(const CVfxFogVolumeInfo**)pA;
	const CVfxFogVolumeInfo& fogVolumeInfoB = **(const CVfxFogVolumeInfo**)pB;

	// sort on event index
	return (fogVolumeInfoA.m_distToCamera>fogVolumeInfoB.m_distToCamera) ? -1 : 1;
}


///////////////////////////////////////////////////////////////////////////////
//  Update
///////////////////////////////////////////////////////////////////////////////

void CVfxFogVolumeInfoMgr::Update()
{
	if (vfxVerifyf(vfxFogVolumeInfos.GetCount()<=FOGVOLUME_MAX_ITEMS, "too many fog volume items - the sort buffer won't work"))
	{
		CVfxFogVolumeInfo* pSortBuffer[FOGVOLUME_MAX_ITEMS];

		// copy the fog volumes into the sort buffer
		for (int i=0; i<vfxFogVolumeInfos.GetCount(); i++)
		{
			vfxFogVolumeInfos[i]->m_distToCamera = Sqrtf(CVfxHelper::GetDistSqrToCamera(vfxFogVolumeInfos[i]->m_position));
			BANK_ONLY(vfxFogVolumeInfos[i]->m_index = i;)
			pSortBuffer[i] = vfxFogVolumeInfos[i];
		}

#if RSG_BANK
		ms_DebugRenderRegisteredIndex = Min(ms_DebugRenderRegisteredIndex, ((int)vfxFogVolumeInfos.GetCount()-1));
		if(ms_DebugRenderRegisteredIndex >= 0)
		{
			if(ms_DebugRenderRegisteredIndexChanged)
			{
				ms_DebugRegisteredFogVolumeOverrideSettings = *(vfxFogVolumeInfos[ms_DebugRenderRegisteredIndex]);
				ms_DebugRenderRegisteredIndexChanged = false;
			}
			if(ms_DebugRenderRegisteredSettingsChanged)
			{
				*(vfxFogVolumeInfos[ms_DebugRenderRegisteredIndex]) = ms_DebugRegisteredFogVolumeOverrideSettings;
				ms_DebugRenderRegisteredSettingsChanged = false;
			}
		}
#endif

		// perform the sort
		qsort(pSortBuffer, vfxFogVolumeInfos.GetCount(), sizeof(const CVfxFogVolumeInfo*), FogVolumeItemSorter);


		// register the sorted fog volumes
		for (int i=0; i<vfxFogVolumeInfos.GetCount(); i++)
		{
			float densityScale = CVfxHelper::GetInterpValue(pSortBuffer[i]->m_distToCamera, pSortBuffer[i]->m_fadeDistStart, pSortBuffer[i]->m_fadeDistEnd);
			densityScale *= ((pSortBuffer[i]->m_interiorHash>0)?g_timeCycle.GetStaleFogVolumeDensityScalarInterior():g_timeCycle.GetStaleFogVolumeDensityScalar());
			BANK_ONLY(bool bDebugCurrentFogVolume = (ms_DebugRenderRegisteredIndex < 0 || pSortBuffer[i]->m_index==ms_DebugRenderRegisteredIndex));
			const bool bUnderwaterCheck = (Water::IsCameraUnderwater() == pSortBuffer[i]->m_isUnderwater);

			if (densityScale>0.0f && pSortBuffer[i]->m_colA>0 && bUnderwaterCheck BANK_ONLY( && (!ms_DebugIsolateCurrentRegisteredVolume || bDebugCurrentFogVolume)))
			{
				FogVolume_t fogVolumeParams;
				fogVolumeParams.pos = pSortBuffer[i]->m_position;
				fogVolumeParams.scale = pSortBuffer[i]->m_scale;
				fogVolumeParams.rotation = pSortBuffer[i]->m_rotation * Vec3V(ScalarV(V_TO_RADIANS));
				fogVolumeParams.range = pSortBuffer[i]->m_range;
				fogVolumeParams.hdrMult = pSortBuffer[i]->m_hdrMult;
				fogVolumeParams.density = pSortBuffer[i]->m_density * densityScale;
				fogVolumeParams.fallOff = pSortBuffer[i]->m_falloff;
				fogVolumeParams.col = Color32(pSortBuffer[i]->m_colR, pSortBuffer[i]->m_colG, pSortBuffer[i]->m_colB, pSortBuffer[i]->m_colA);
				fogVolumeParams.lightingType = (FogVolumeLighting_e)pSortBuffer[i]->m_lightingType;
				fogVolumeParams.useGroundFogColour = false;
				fogVolumeParams.interiorHash = pSortBuffer[i]->m_interiorHash;
				fogVolumeParams.fogMult = g_timeCycle.GetStaleFogVolumeFogScalar();
				BANK_ONLY(fogVolumeParams.debugRender = bDebugCurrentFogVolume;)

				g_fogVolumeMgr.Register(fogVolumeParams);
			}
		}
	}
}


#if RSG_BANK

void CVfxFogVolumeInfoMgr::PrintFogVolumeParams(const CVfxFogVolumeInfo* pFogVolumeInfo)
{
	Displayf("Fog Volume Params - Copy Text between lines");
	Displayf("-----------------------------------------------------------------------------------------------");
	Displayf("<Item type=\"CVfxFogVolumeInfo\" />");
	Displayf("\t<fadeDistStart value=\"%9.7f\" />", pFogVolumeInfo->m_fadeDistStart);
	Displayf("\t<fadeDistEnd value=\"%9.7f\" />", pFogVolumeInfo->m_fadeDistEnd);
	Displayf("\t<position x=\"%9.7f\" y=\"%9.7f\" z=\"%9.7f\" />", pFogVolumeInfo->m_position.GetXf(), pFogVolumeInfo->m_position.GetYf(), pFogVolumeInfo->m_position.GetZf());
	Displayf("\t<rotation x=\"%9.7f\" y=\"%9.7f\" z=\"%9.7f\" />", pFogVolumeInfo->m_rotation.GetXf(), pFogVolumeInfo->m_rotation.GetYf(), pFogVolumeInfo->m_rotation.GetZf());
	Displayf("\t<scale x=\"%9.7f\" y=\"%9.7f\" z=\"%9.7f\" />", pFogVolumeInfo->m_scale.GetXf(), pFogVolumeInfo->m_scale.GetYf(), pFogVolumeInfo->m_scale.GetZf());
	Displayf("\t<colR value=\"%u\" />", pFogVolumeInfo->m_colR);
	Displayf("\t<colG value=\"%u\" />", pFogVolumeInfo->m_colG);
	Displayf("\t<colB value=\"%u\" />", pFogVolumeInfo->m_colB);
	Displayf("\t<colA value=\"%u\" />", pFogVolumeInfo->m_colA);
	Displayf("\t<hdrMult value=\"%9.7f\" />", pFogVolumeInfo->m_hdrMult);
	Displayf("\t<range value=\"%9.7f\" />", pFogVolumeInfo->m_range);
	Displayf("\t<density value=\"%9.7f\" />", pFogVolumeInfo->m_density);
	Displayf("\t<falloff value=\"%9.7f\" />", pFogVolumeInfo->m_falloff);
	Displayf("\t<interiorHash value=\"0x%" I64FMT "x\" />", pFogVolumeInfo->m_interiorHash);
	Displayf("\t<isUnderwater value=\"%s\" />", pFogVolumeInfo->m_isUnderwater?"true":"false");
	static const char* lightingTypes[] =
	{
		"FOGVOLUME_LIGHTING_FOGHDR",
		"FOGVOLUME_LIGHTING_DIRECTIONAL",
		"FOGVOLUME_LIGHTING_NONE"
	};

	CompileTimeAssert(NELEM(lightingTypes) == FOGVOLUME_LIGHTING_NUM);
	Displayf("\t<lightingType>%s</lightingType>", lightingTypes[pFogVolumeInfo->m_lightingType]);
	Displayf("</Item>");
	Displayf("-----------------------------------------------------------------------------------------------");
}

void CVfxFogVolumeInfoMgr::ResetOverrideDebugRegisteredFogVolume()
{
	ms_DebugRenderRegisteredIndexChanged = true;
}

void CVfxFogVolumeInfoMgr::ResetOverrideDebugSettingsChanged()
{
	ms_DebugRenderRegisteredSettingsChanged = true;
}

void CVfxFogVolumeInfoMgr::PrintCurrentFogVolumeParams()
{
	g_vfxFogVolumeInfoMgr.PrintFogVolumeParams(&ms_DebugRegisteredFogVolumeOverrideSettings);
}

void CVfxFogVolumeInfoMgr::Reset()
{
	g_vfxFogVolumeInfoMgr.vfxFogVolumeInfos.clear();
	g_vfxFogVolumeInfoMgr.LoadData();

}

void CVfxFogVolumeInfoMgr::InitWidgets()
{

	bkBank* pVfxBank = vfxWidget::GetBank();

	pVfxBank->PushGroup("World Fog Volume Infos");
	pVfxBank->AddSlider("Render Register Fog Volume Index", &(ms_DebugRenderRegisteredIndex), -1, FOGVOLUME_MAX_ITEMS-1, 1, ResetOverrideDebugRegisteredFogVolume);
	pVfxBank->AddToggle("Isolate Current Registered Fog Volume", &ms_DebugIsolateCurrentRegisteredVolume);
	pVfxBank->AddButton("Reload Data", Reset);

	pVfxBank->AddTitle("");
	pVfxBank->AddTitle("Current Registered Fog Volume Details");
	pVfxBank->AddButton("Print Current Fog Volume Parameters", PrintCurrentFogVolumeParams);
	pVfxBank->AddSlider("Fade Dist Start", &ms_DebugRegisteredFogVolumeOverrideSettings.m_fadeDistStart, 0.0f, 10000.0f, 0.1f, ResetOverrideDebugSettingsChanged);
	pVfxBank->AddSlider("Fade Dist End", &ms_DebugRegisteredFogVolumeOverrideSettings.m_fadeDistEnd, 0.0f, 10000.0f, 0.1f, ResetOverrideDebugSettingsChanged);
	pVfxBank->AddVector("Position", &ms_DebugRegisteredFogVolumeOverrideSettings.m_position, -10000.0f, 10000.0f, 0.1f, ResetOverrideDebugSettingsChanged);
	pVfxBank->AddVector("Rotation", &ms_DebugRegisteredFogVolumeOverrideSettings.m_rotation, -360.0f, 360.0f, 0.01f, ResetOverrideDebugSettingsChanged);
	pVfxBank->AddVector("Scale", &ms_DebugRegisteredFogVolumeOverrideSettings.m_scale, 0.0f, 1.0f, 0.001f, ResetOverrideDebugSettingsChanged);
	pVfxBank->AddSlider("Color Red", &ms_DebugRegisteredFogVolumeOverrideSettings.m_colR, 0, 255, 1, ResetOverrideDebugSettingsChanged);
	pVfxBank->AddSlider("Color Green", &ms_DebugRegisteredFogVolumeOverrideSettings.m_colG, 0, 255, 1, ResetOverrideDebugSettingsChanged);
	pVfxBank->AddSlider("Color Blue", &ms_DebugRegisteredFogVolumeOverrideSettings.m_colB, 0, 255, 1, ResetOverrideDebugSettingsChanged);
	pVfxBank->AddSlider("Density", &ms_DebugRegisteredFogVolumeOverrideSettings.m_density, 0.0f, 100.0f, 0.001f, ResetOverrideDebugSettingsChanged);
	pVfxBank->AddSlider("Range", &ms_DebugRegisteredFogVolumeOverrideSettings.m_range, 0.0f, 10000.0f, 0.1f, ResetOverrideDebugSettingsChanged);
	pVfxBank->AddSlider("FallOff", &ms_DebugRegisteredFogVolumeOverrideSettings.m_falloff, 0.0f, 256.0f, 1.0f, ResetOverrideDebugSettingsChanged);
	pVfxBank->AddSlider("HDR Mult", &ms_DebugRegisteredFogVolumeOverrideSettings.m_hdrMult, 0.0f, 100.0f, 0.001f, ResetOverrideDebugSettingsChanged);

	const char* lightingTypes[] =
	{
		"Fog HDR",
		"Directional Lighting",
		"No Lighting"
	};
	CompileTimeAssert(NELEM(lightingTypes) == FOGVOLUME_LIGHTING_NUM);
	pVfxBank->AddCombo("Lighting Type", (unsigned char*)(&ms_DebugRegisteredFogVolumeOverrideSettings.m_lightingType), NELEM(lightingTypes), lightingTypes);

	pVfxBank->PopGroup();

}
#endif