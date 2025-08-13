//
// filename:	EntitySelect.h
// description: EntitySelect class
//

#ifndef INC_ENTITYSELECT_H_
#define INC_ENTITYSELECT_H_

#include "entity/entity.h"
#include "scene/Entity.h"
#include "scene/EntityId.h"
#include "scene/EntityTypes.h"
#include "system/memory.h"

//ENTITYSELECT_ENABLED_BUILD is defined in RenderPhaseEntitySelect.h. That's the only reason for including it here.
#include "renderer/RenderPhases/RenderPhaseEntitySelect.h"

#if ENTITYSELECT_ENABLED_BUILD
namespace rage
{
	class bkBank;
	class grmShaderGroup;
}

class CEntitySelect : public rage::datBase
{
public:
	typedef CEntitySelect this_type;

	//Update
	static void Update();

	// Init takes pointers to a fullscreen render target and depth buffer, both of which will be rendered to. The render target must be
	// grctfA8R8G8B8 format and must be the size of the backbuffer. Passing these targets in allows conservation of memory. However, if no targets
	// available for such use, you can leave these as NULL and targets will automatically be created.
	//
	// NOTE: On Xenon, the rgb and depth targets are created so that they exist on EDRAM only! Since this does not require any extra space, it may
	// be beneficial to allow EntitySelect to create and manage these targets on it's own. However, on xbox it's still very important to execute this
	// draw pass at the right time since the render phase will overwrite whatever is currently in EDRAM.
	static void Init(grcRenderTarget *fullsizeRGBTarget = NULL, grcRenderTarget *fullsizeDepthTarget = NULL);
	static void ShutDown();

	//Render Target Management
	static void LockTargets();
	static void UnlockTargets();

	static void BeginExternalPass();
	static void EndExternalPass();

	//Drawlist Setup
	static void PrepareGeometryPass();
	static void FinishGeometryPass();
	static void ClearResolveTargets();

	//Entity Selection Pass Enabled/Disabled
	static bool IsEntitySelectPassEnabled()		{ return sEntitySelectRenderPassEnabled; }
	static void SetEntitySelectPassEnabled()	{ sEntitySelectRenderPassEnabled = true; }
	static void DisableEntitySelectPass()		{ sEntitySelectRenderPassEnabled = false; }

	//Debug Draw Pass
	static void GetSelectScreenQuad(Vector2& pos, Vector2& size);
	static void DrawDebug();

	//Functions for entities.
	static entitySelectID   GetSelectedEntityId();
	static fwEntity*        GetSelectedEntity();
	static int              GetSelectedEntityShaderIndex();
	static const grmShader* GetSelectedEntityShader(); // NOTE -- this will return NULL for peds
	static const char*      GetSelectedEntityShaderName();

	static void BindEntityIDShaderParam(entitySelectID id);
	static bool BindEntityIDShaderParamFunc(const grmShaderGroup& group, int shaderIndex); // called per shader
	static grcEffectGlobalVar GetEntityIDShaderVarID();

	static void AddWidgets(bkGroup &bank);

	static void SetRenderStates();
	
private:
	static bool sEntitySelectRenderPassEnabled;
	static bool sEntitySelectUsingShaderIndexExtension;
	static bool sEntitySelectEnablePedShaderHack;

public:
	static bool IsUsingShaderIndexExtension() { return sEntitySelectUsingShaderIndexExtension; }
};
#endif //ENTITYSELECT_ENABLED_BUILD

#endif //INC_ENTITYSELECT_H_
