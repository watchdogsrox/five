///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////

#include "ScriptIM.h"

#include "grcore/allocscope.h"
#include "grcore/light.h"

#include "fwsys/gameskeleton.h"
#include "vfx/vfxwidget.h"
#include "Frontend/MobilePhone.h"

#include "debug/DebugDraw.h"
#include "renderer/Drawlists/drawList.h"
#include "shaders/ShaderLib.h"

VFX_MISC_OPTIMISATIONS()

#define SIM_MAX_COMMAND_COUNT (8)
#define SIM_BUFFER_SIZE (sizeof(ScriptIM::drawCommand) * SIM_MAX_COMMAND_COUNT)

namespace ScriptIM {

enum command {
	DrawLine,
	DrawPoly,
	DrawPoly3Colours,
	DrawPolyTextured,
	DrawPolyTextured3Colours,
	DrawBox,
	DrawSprite3d,
	SetBackFaceCullingOff,
	SetBackFaceCullingOn,
	SetDepthWritingOff,
	SetDepthWritingOn,
	Next,
	End,
};
	
struct drawCommand {
	Vec3V p1;
	Vec3V p2;
	Vec3V p3;
	Vector2 uv1;
	Vector2 uv2;
	Vector2 uv3;
	grcTexture *tex;
	Color32 col1;
	Color32 col2;
	Color32 col3;
	command type;
	drawCommand *next;
} ;

static drawCommand* firstBuffer;
static drawCommand* currentBuffer;
static int currentBufferIdx;

#if __BANK
static bool displayDebug = false;
static int bufferCount;
static int polyCount;
static int lineCount;
static int boxCount;
static int stateCount;
#endif // __BANK

};


class dlCmdDrawScriptIM : public dlCmdBase {
public:
	enum {
		INSTRUCTION_ID = DC_DrawScriptIM,
	};

	s32 GetCommandSize()						{ return(sizeof(*this)); }
	dlCmdDrawScriptIM(ScriptIM::drawCommand *in) 
	{ 
		buffer = in;
	}
	
	static void ExecuteStatic(dlCmdBase &cmd) { ((dlCmdDrawScriptIM &) cmd).Execute(); }
	
	void Execute()
	{ 
		GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

		grcBindTexture(NULL);
		grcWorldIdentity();

		grcLightState::SetEnabled(false);
		grcStateBlock::SetBlendState(grcStateBlock::BS_Normal);
		grcStateBlock::SetDepthStencilState(CShaderLib::DSS_TestOnly_Invert);
		grcStateBlock::SetRasterizerState(grcStateBlock::RS_Default);
	
		ScriptIM::drawCommand *command = buffer;
		while( command )
		{
			switch( command->type )
			{
				case ScriptIM::DrawLine:
					{
						const Vec3V p1 = command->p1;
						const Vec3V p2 = command->p2;
						const Color32 color = command->col1;
						grcDrawLine(RCC_VECTOR3(p1),RCC_VECTOR3(p2),color);
					}				
					command++;
					break;

				case ScriptIM::DrawPoly:
					{
						Vec3V points[3];
						points[0] = command->p1;
						points[1] = command->p2;
						points[2] = command->p3;

						Color32 color = command->col1;
						grcDrawPolygon((Vector3 *) points, 3, NULL, true, color);
					}				
					command++;
					break;

				case ScriptIM::DrawPoly3Colours:
					{
						Vec3V points[3];
						points[0] = command->p1;
						points[1] = command->p2;
						points[2] = command->p3;

						Color32 colors[3];
						colors[0] = command->col1;
						colors[1] = command->col2;
						colors[2] = command->col3;
						
						grcDrawPolygon((Vector3*)points, 3, NULL, true, colors);
					}				
					command++;
					break;

				case ScriptIM::DrawPolyTextured:
					{
						Vec3V points[3];
						points[0] = command->p1;
						points[1] = command->p2;
						points[2] = command->p3;

						Color32 color = command->col1;

						grcTexture *pTex = command->tex;

						Vector2 uvs[3];
						uvs[0]	  = command->uv1;
						uvs[1]	  = command->uv2;
						uvs[2]	  = command->uv3;

						grcDrawTexturedPolygon((Vector3*)points, 3, NULL, true, color, pTex, uvs);
					}				
					command++;
					break;

				case ScriptIM::DrawPolyTextured3Colours:
					{
						Vec3V points[3];
						points[0] = command->p1;
						points[1] = command->p2;
						points[2] = command->p3;

						Color32 colors[3];
						colors[0] = command->col1;
						colors[1] = command->col2;
						colors[2] = command->col3;

						grcTexture *pTex = command->tex;

						Vector2 uvs[3];
						uvs[0]	  = command->uv1;
						uvs[1]	  = command->uv2;
						uvs[2]	  = command->uv3;

						grcDrawTexturedPolygon((Vector3*)points, 3, NULL, true, colors, pTex, uvs);
					}				
					command++;
					break;

				case ScriptIM::DrawBox:
					{
						Vec3V min = command->p1;
						Vec3V max = command->p2;
						Color32 color = command->col1;
						
						grcDrawSolidBox(RCC_VECTOR3(min),RCC_VECTOR3(max),color);
						grcWorldIdentity();
					}				
					command++;
					break;

				case ScriptIM::DrawSprite3d:
					{
						const Vec3V& pos = command->p1;
						const float x = pos.GetXf();
						const float y = pos.GetYf();
						const float z = pos.GetZf();
					
						const Vec3V& dimensions = command->p2;
						const float width  = dimensions.GetXf();
						const float height = dimensions.GetYf();

						Color32 color		= command->col1;
						grcTexture *pTex	= command->tex;
						const Vector2& uv0	= command->uv1;
						const Vector2& uv1	= command->uv2;

						CSprite2d::DrawSprite3D(x, y, z, width, height, color, pTex, uv0, uv1);
					}
					command++;
					break;

				case ScriptIM::SetBackFaceCullingOff:
					{
						grcStateBlock::SetRasterizerState(grcStateBlock::RS_NoBackfaceCull);
					}
					command++;
					break;

				case ScriptIM::SetBackFaceCullingOn:
					{
						grcStateBlock::SetRasterizerState(grcStateBlock::RS_Default);
					}				
					command++;
					break;

				case ScriptIM::SetDepthWritingOff:
					{
						grcStateBlock::SetDepthStencilState(CShaderLib::DSS_TestOnly_Invert);
					}
					command++;
					break;

				case ScriptIM::SetDepthWritingOn:
					{
						grcStateBlock::SetDepthStencilState(CShaderLib::DSS_Default_Invert);
					}
					command++;
					break;

				case ScriptIM::Next:
					command = command->next;
					break;

				case ScriptIM::End:
					command = NULL;
					break;

				default:
					Assert( false );
					command = NULL;
					break;
			}
		}		
	}
	
private:
	ScriptIM::drawCommand *buffer;
};

static bool SIMGetSpace()
{
	if( ScriptIM::currentBuffer && ScriptIM::currentBufferIdx + 1 < SIM_MAX_COMMAND_COUNT) // Leave one extra slot for next/stop command
	{
		return true;
	}
	
	// current Buffer is full, move on to the next.
	ScriptIM::drawCommand* newBuffer = gDCBuffer->AllocateObjectFromPagedMemory<ScriptIM::drawCommand>(DPT_LIFETIME_RENDER_THREAD, SIM_BUFFER_SIZE, true);
	if( newBuffer )
	{
#if __BANK
		ScriptIM::bufferCount++;
#endif // __BANK

		if( ScriptIM::currentBuffer )
		{
			ScriptIM::currentBuffer[ScriptIM::currentBufferIdx].type = ScriptIM::Next;
			ScriptIM::currentBuffer[ScriptIM::currentBufferIdx].next = newBuffer;
		}
		else
		{
			ScriptIM::firstBuffer = newBuffer;
		}
		
		ScriptIM::currentBuffer = newBuffer;
		ScriptIM::currentBufferIdx = 0;
		
		return true;
	}
	
	return false;
}

void ScriptIM::Init(unsigned initMode)
{
    if(initMode == INIT_CORE)
    {
		DLC_REGISTER_EXTERNAL(dlCmdDrawScriptIM);

		Clear();
    }
}

void ScriptIM::Clear()
{
	currentBuffer = NULL;
	firstBuffer = NULL;
	currentBufferIdx = 0;
}

void ScriptIM::Render()
{
	if( currentBuffer ) 
	{
		bool doRender = !CPhoneMgr::CamGetState();
		Assert(currentBufferIdx < SIM_MAX_COMMAND_COUNT );

		// Close the last batch
		currentBuffer[currentBufferIdx].type = ScriptIM::End;
		currentBufferIdx++;

		if(doRender)
			DLC (dlCmdDrawScriptIM, (firstBuffer));
		
		Clear();
	}

#if __BANK
	if( ScriptIM::displayDebug )
	{
		grcDebugDraw::AddDebugOutput("ScIM %d(%d) - %d %d %d %d",ScriptIM::bufferCount, ScriptIM::bufferCount * SIM_BUFFER_SIZE, ScriptIM::polyCount, ScriptIM::lineCount, ScriptIM::boxCount, ScriptIM::stateCount);
		ScriptIM::bufferCount = 0;
		ScriptIM::polyCount = 0;
		ScriptIM::lineCount = 0;
		ScriptIM::boxCount = 0;
		ScriptIM::stateCount = 0;
	}
#endif // __BANK
	
}

void ScriptIM::Line(Vec3V_In start, Vec3V_In end, const Color32 color)
{
	if( SIMGetSpace() )
	{
		currentBuffer[currentBufferIdx].type = ScriptIM::DrawLine;
		currentBuffer[currentBufferIdx].p1 = start;
		currentBuffer[currentBufferIdx].p2 = end;
		currentBuffer[currentBufferIdx].col1 = color;
		currentBufferIdx++;

#if __BANK
		ScriptIM::lineCount++;
#endif // __BANK

	}
}


void ScriptIM::Poly(Vec3V_In P1, Vec3V_In P2, Vec3V_In P3, const Color32 color)
{
	if( SIMGetSpace() )
	{
		currentBuffer[currentBufferIdx].type = ScriptIM::DrawPoly;
		currentBuffer[currentBufferIdx].p1 = P1;
		currentBuffer[currentBufferIdx].p2 = P2;
		currentBuffer[currentBufferIdx].p3 = P3;
		currentBuffer[currentBufferIdx].col1 = color;

		currentBufferIdx++;
#if __BANK
		ScriptIM::polyCount++;
#endif // __BANK
	}
}

void ScriptIM::Poly(Vec3V_In P1, Vec3V_In P2, Vec3V_In P3, const Color32 color1, const Color32 color2, const Color32 color3)
{
	if( SIMGetSpace() )
	{
		currentBuffer[currentBufferIdx].type = ScriptIM::DrawPoly3Colours;
		currentBuffer[currentBufferIdx].p1 = P1;
		currentBuffer[currentBufferIdx].p2 = P2;
		currentBuffer[currentBufferIdx].p3 = P3;
		currentBuffer[currentBufferIdx].col1 = color1;
		currentBuffer[currentBufferIdx].col2 = color2;
		currentBuffer[currentBufferIdx].col3 = color3;

		currentBufferIdx++;
#if __BANK
		ScriptIM::polyCount++;
#endif // __BANK
	}
}

void ScriptIM::PolyTex(Vec3V_In P1, Vec3V_In P2, Vec3V_In P3, const Color32 color, grcTexture *pTex, const Vector2& UV1, const Vector2& UV2, const Vector2& UV3)
{
	if( SIMGetSpace() )
	{
		currentBuffer[currentBufferIdx].type = ScriptIM::DrawPolyTextured;
		currentBuffer[currentBufferIdx].p1 = P1;
		currentBuffer[currentBufferIdx].p2 = P2;
		currentBuffer[currentBufferIdx].p3 = P3;

		currentBuffer[currentBufferIdx].tex= pTex;
		currentBuffer[currentBufferIdx].uv1= UV1;
		currentBuffer[currentBufferIdx].uv2= UV2;
		currentBuffer[currentBufferIdx].uv3= UV3;

		currentBuffer[currentBufferIdx].col1 = color;

		currentBufferIdx++;
#if __BANK
		ScriptIM::polyCount++;
#endif // __BANK
	}
}

void ScriptIM::PolyTex(Vec3V_In P1, Vec3V_In P2, Vec3V_In P3, const Color32 color1, const Color32 color2, const Color32 color3,
						grcTexture *pTex, const Vector2& UV1, const Vector2& UV2, const Vector2& UV3)
{
	if( SIMGetSpace() )
	{
		currentBuffer[currentBufferIdx].type = ScriptIM::DrawPolyTextured3Colours;
		currentBuffer[currentBufferIdx].p1 = P1;
		currentBuffer[currentBufferIdx].p2 = P2;
		currentBuffer[currentBufferIdx].p3 = P3;

		currentBuffer[currentBufferIdx].tex= pTex;
		currentBuffer[currentBufferIdx].uv1= UV1;
		currentBuffer[currentBufferIdx].uv2= UV2;
		currentBuffer[currentBufferIdx].uv3= UV3;

		currentBuffer[currentBufferIdx].col1 = color1;
		currentBuffer[currentBufferIdx].col2 = color2;
		currentBuffer[currentBufferIdx].col3 = color3;

		currentBufferIdx++;
#if __BANK
		ScriptIM::polyCount++;
#endif // __BANK
	}
}

void ScriptIM::BoxAxisAligned(Vec3V_In min, Vec3V_In max, const Color32 color)
{
	if( SIMGetSpace() )
	{
		currentBuffer[currentBufferIdx].type = ScriptIM::DrawBox;
		currentBuffer[currentBufferIdx].p1 = min;
		currentBuffer[currentBufferIdx].p2 = max;
		currentBuffer[currentBufferIdx].col1 = color;

		currentBufferIdx++;
#if __BANK
		ScriptIM::boxCount++;
#endif // __BANK
	}
}

void ScriptIM::Sprite3D(Vec3V_In pos, Vec3V_In dimensions, const Color32 color, grcTexture *pTex, const Vector2& uv0, const Vector2& uv1)
{
	if( SIMGetSpace() )
	{
		currentBuffer[currentBufferIdx].type	= ScriptIM::DrawSprite3d;
		currentBuffer[currentBufferIdx].p1		= pos;
		currentBuffer[currentBufferIdx].p2		= dimensions;
		currentBuffer[currentBufferIdx].tex		= pTex;
		currentBuffer[currentBufferIdx].uv1		= uv0;
		currentBuffer[currentBufferIdx].uv2		= uv1;
		currentBuffer[currentBufferIdx].col1	= color;

		currentBufferIdx++;
#if __BANK
		ScriptIM::polyCount++;
#endif // __BANK
	}
}

void ScriptIM::SetBackFaceCulling(bool on)
{
	if( SIMGetSpace() )
	{
		currentBuffer[currentBufferIdx].type = on ? ScriptIM::SetBackFaceCullingOn : ScriptIM::SetBackFaceCullingOff;
		currentBufferIdx++;
#if __BANK
		ScriptIM::stateCount++;
#endif // __BANK
	}
}

void ScriptIM::SetDepthWriting(bool on)
{
	if( SIMGetSpace() )
	{
		currentBuffer[currentBufferIdx].type = on ? ScriptIM::SetDepthWritingOn : ScriptIM::SetDepthWritingOff;
		currentBufferIdx++;
#if __BANK
		ScriptIM::stateCount++;
#endif // __BANK
	}
}


#if __BANK
void ScriptIM::InitWidgets()
{
	bkBank* pVfxBank = vfxWidget::GetBank();
	pVfxBank->PushGroup("ScriptIM", false);
	{
		pVfxBank->AddToggle("Draw Stats",&ScriptIM::displayDebug);

	}
	pVfxBank->PopGroup();
}
#endif // __BANK

