// ======================
// renderer/util/shmoofile.cpp
// (c) 2010 RockstarNorth
// ======================

#if __PS3
#include "parser/manager.h"
#include "system/param.h"
#include "bank/bank.h"
#include "grcore/channel.h"
#include "grmodel/shader.h"

#include "renderer/util/shmoofile.h"

#include "ShmooFile_parser.h"

namespace rage {
NOSTRIP_XPARAM(useFinalShaders); 
NOSTRIP_XPARAM(useOptimizedShaders); 
};

static void formatFileName(char *filename,const char *shadername)
{
	bool bUseFinalShaders = __FINAL ? true : false;
	if ( PARAM_useFinalShaders.Get() )
	{
		int nParam = 0;
		PARAM_useFinalShaders.Get(nParam);
		bUseFinalShaders = nParam ? true : false;
	}

	bool bUseOptimizedShaders = __FINAL ? true : false;
	if ( PARAM_useOptimizedShaders.Get() )
	{
		int nParam = 0;
		PARAM_useOptimizedShaders.Get(nParam);
		bUseOptimizedShaders = nParam ? true : false;
	}

	const char *platformString = "psn";
	
	char pStrPlatformFolder[32];
	if ( bUseFinalShaders || bUseOptimizedShaders )
	{
		Assert( (strlen(platformString) + sizeof("_final") PS3_ONLY(+ sizeof("_optimized"))) <= sizeof(pStrPlatformFolder) );
		strncpy( pStrPlatformFolder, platformString, sizeof(pStrPlatformFolder) );

		if ( bUseOptimizedShaders )
			strncat( pStrPlatformFolder, "_optimized", sizeof("_optimized") );

		if ( bUseFinalShaders )
			strncat( pStrPlatformFolder, "_final", sizeof("_final") );

		platformString = &pStrPlatformFolder[0];
	}

	sprintf(filename,"common:/shaders/%s/%s",platformString,shadername);
}

#if __BANK
void ShmooFile::Set(const atArray<grcShmoo::Result> &results)
{
	for(int i=0;i<results.GetCount();i++)
	{
		ShmooItem &item = m_ShmooItems.Grow();

		item.m_TechniqueName = results[i].TechniqueName;
		item.m_Pass = results[i].PassIndex;
		item.m_RegisterCount = results[i].RegisterCount;
	}
}

bool ShmooFile::Save(const char *shadername)
{
	char filename[1024];
	formatFileName(filename,shadername);
	
	return PARSER.SaveObject(filename, "xml", this);
}

#endif // __BANK

bool ShmooFile::Load(const char* shadername)
{
	bool result = false;
	
	char filename[1024];
	formatFileName(filename,shadername);
	if (ASSET.Exists(filename, "xml"))
		result = PARSER.LoadObject(filename, "xml", *this);
		
	return result;
}

void ShmooFile::Apply(grmShader *shader)
{
	atArray<grcShmoo::Result> results;
	
	for(int i=0;i<m_ShmooItems.GetCount();i++)
	{
		grcShmoo::Result &res = results.Grow();

		res.TechniqueName = m_ShmooItems[i].m_TechniqueName;
		res.PassIndex = m_ShmooItems[i].m_Pass;
		res.RegisterCount = m_ShmooItems[i].m_RegisterCount;
	}
	
	grcShmoo::ApplyResults(shader,results);
}

static atArray<grcShmoo *> shmooArray;
static atArray<float> deltaArray;

int ShmooHandling::Register(const char* shaderName, grmShader *shader, bool shouldExist, float improvementDelta)
{
	ShmooFile file;

	bool loaded = file.Load(shaderName);
	if( loaded )
	{
		grcDisplayf("Loaded %s Shmoo file",shaderName);
		file.Apply(shader);
	}
	

	if( shouldExist && loaded == false )
	{
		grcWarningf("Failed to load %s shmoo file",shaderName);
	}	

#if GENERATE_SHMOO
	int instanceCount = 0;
	int techCount = shader->GetTechniqueCount();
	
	for(int i=0;i<techCount;i++)
	{
		grcEffectTechnique tech = shader->GetTechniqueByIndex(i);
		instanceCount += shader->GetPassCount(tech);
	}
		
	grcShmoo * shm = rage_new grcShmoo(shaderName,instanceCount);
	shmooArray.PushAndGrow(shm);	
	deltaArray.PushAndGrow(improvementDelta);
	
	return shmooArray.GetCount() - 1;
#else
	return -1;
#endif	
}

#if GENERATE_SHMOO

bool ShmooHandling::IsShmooEnable(int shmooIdx)
{
	if( shmooIdx > -1 )
		return shmooArray[shmooIdx]->IsEnabled();

	return false;
}
void ShmooHandling::BeginShmoo(int shmooIdx, const grmShader *shader, int pass)
{
	if( shmooIdx > -1 )
		shmooArray[shmooIdx]->Begin(shader, pass);
}

void ShmooHandling::EndShmoo(int shmooIdx)
{
	if( shmooIdx > -1 )
		shmooArray[shmooIdx]->End();
}

#if __BANK
static void SaveShmooFiles()
{
	for(int i=0;i<shmooArray.GetCount();i++)
	{
		if( shmooArray[i]->IsEnabled() )
		{
			atArray<grcShmoo::Result> results;
			shmooArray[i]->GetResults(results,deltaArray[i]);

			ShmooFile file;
			file.Set(results);

			bool res = file.Save(shmooArray[i]->GetName());
			if( res )
				grcDisplayf("Saved %s shmoofile", shmooArray[i]->GetName().c_str());
				
			Assertf(res, "Failed to save %s shmoofile", shmooArray[i]->GetName().c_str());
		}
	}
}

void ShmooHandling::AddWidgets(bkBank& bank)
{
	bank.PushGroup("Shmoo");
	bank.AddButton("Save Shmoo Files",CFA1(SaveShmooFiles));

	for(int i=0;i<shmooArray.GetCount();i++)
	{
		shmooArray[i]->AddWidgets(bank);
	}
	bank.PopGroup();
}
#endif // __BANK

#endif // GENERATE_SHMOO

#endif // __PS3
