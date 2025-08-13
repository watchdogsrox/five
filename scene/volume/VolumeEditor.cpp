/////////////////////////////////////////////////////////////////////////////////
// FILE :		VolumeEditor.cpp
// PURPOSE :	An Editor for volume manipulation
// AUTHOR :		Jason Jurecka.
// CREATED :	10/21/2011
/////////////////////////////////////////////////////////////////////////////////
#include "Scene/Volume/VolumeEditor.h"

#if __BANK && VOLUME_SUPPORT

///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////

//rage
#include "input/mouse.h"
#include "input/keyboard.h"
#include "input/keys.h"
#include "physics/debugPlayback.h"
#include "atl/binmap.h"

//framework

//Game
#include "camera/CamInterface.h"
#include "camera/viewports/viewportmanager.h"
#include "Scene/Volume/VolumeAggregate.h"
#include "Scene/Volume/VolumePrimitiveBase.h"
#include "Scene/Volume/VolumeManager.h"
#include "Scene/Volume/VolumeTool.h"
#include "script/script.h"
#include "script/handlers/GameScriptResources.h"

///////////////////////////////////////////////////////////////////////////////
//  GLOBAL VARIABLES
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  CODE
///////////////////////////////////////////////////////////////////////////////
CVolumeEditor::CVolumeEditor() 
: m_isActive(false)
, m_LocalRotations(false)
, m_TransWorldAligned(false)
, m_isHoveredOnText(false)
, m_ShowNameOfHovered(true)
, m_SnapVolumes(false)
, m_prevMouseX(0)
, m_prevMouseY(0)
, m_scaleStep(0.05f)
, m_transStep(0.01f)
, m_rotateStep(1.0f)
, m_ScriptHandler(NULL)
, m_HoveredObject(NULL)
, m_EditorObjectsHead(NULL)
{
	m_EditorWidgetDelegate.Bind(this, &CVolumeEditor::OnEditorWidgetEvent);
}

void CVolumeEditor::Init(CGameScriptHandler* handler)
{
	m_isActive = true;
	m_HoveredObject = NULL;
	m_EditorObjectsHead = NULL;
	m_SelectedObjects.Reset();
	m_ScriptHandler = handler;
	m_ScriptHandler->ForAllScriptResources(&CVolumeEditor::AddScriptResourceToEditorCB, this);
	m_EditorWidget.m_Delegator.AddDelegate(&m_EditorWidgetDelegate);
}

void CVolumeEditor::Shutdown()
{	
	SYS_CS_SYNC(m_criticalSection);
	m_isActive = false;
	m_ScriptHandler = NULL;
	m_HoveredObject = NULL;
	m_SelectedObjects.Reset();

	//Delete all the editor objects ...
	EditorObject* current = m_EditorObjectsHead;
	while (current)
	{
		EditorObject* toDelete = current;
		current = current->m_Next;
		delete toDelete;
	}
	m_EditorObjectsHead = NULL;
	m_EditorWidget.m_Delegator.RemoveDelegate(&m_EditorWidgetDelegate);
}

void CVolumeEditor::Update()
{
	//Update Selection
	SYS_CS_SYNC(m_criticalSection);
	if ((ioMouse::GetPressedButtons() & ioMouse::MOUSE_LEFT ) && 
		!m_EditorWidget.WantHoverFocus()
		)
	{
		if (m_isHoveredOnText)
		{
			Assert(m_HoveredObject);
			Assert(m_HoveredObject->m_Volume);

			if (m_HoveredObject->m_Volume->IsAggregateVolume())
			{
				if (m_SelectedObjects.Access((unsigned)m_HoveredObject->m_Volume))
				{
					//remove myself from being selected.
					m_SelectedObjects.Delete((unsigned)m_HoveredObject->m_Volume);

					//DeSelect the children ... 
					EditorObject* current = m_EditorObjectsHead;
					while (current)
					{
						Assert(current->m_Volume);
						if (current->m_Volume->GetOwner() == m_HoveredObject->m_Volume)
						{
							//DeSelect the volume
							m_SelectedObjects.Delete((unsigned)current->m_Volume);
						}
						current = current->m_Next;
					}
				}
				else
				{
					if (!ioMapper::DebugKeyDown(KEY_SHIFT))
						ClearSelected();

					//Select the aggregate ...
					m_SelectedObjects.Insert((unsigned)m_HoveredObject->m_Volume, m_HoveredObject);

					//Select the children ... 
					EditorObject* current = m_EditorObjectsHead;
					while (current)
					{
						Assert(current->m_Volume);
						if (current->m_Volume->GetOwner() == m_HoveredObject->m_Volume)
						{
							//Select the volume
							m_SelectedObjects.Insert((unsigned)current->m_Volume, current);
						}
						current = current->m_Next;
					}
				}
			}
			else
			{
				if (!m_SelectedObjects.Access((unsigned)m_HoveredObject->m_Volume))
				{
					if (!ioMapper::DebugKeyDown(KEY_SHIFT))
						ClearSelected();

					//Select the volume
					m_SelectedObjects.Insert((unsigned)m_HoveredObject->m_Volume, m_HoveredObject);
				}
				else 
				{
					//De-Select the volume
					m_SelectedObjects.Delete((unsigned)m_HoveredObject->m_Volume);
				}
			}
		}
		else
		{

			if (m_HoveredObject)
			{
				if (!m_SelectedObjects.Access((unsigned)m_HoveredObject->m_Volume))
				{
					if (!ioMapper::DebugKeyDown(KEY_SHIFT))
						ClearSelected();

					//Select the volume
					m_SelectedObjects.Insert((unsigned)m_HoveredObject->m_Volume, m_HoveredObject);
				}
				else 
				{
					//De-Select the volume
					m_SelectedObjects.Delete((unsigned)m_HoveredObject->m_Volume);
				}
			}
			else
			{
				if (!ioMapper::DebugKeyDown(KEY_SHIFT))
					ClearSelected();
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	//Event updates
	Mat34V matrix(V_IDENTITY);
	if (m_SelectedObjects.GetNumUsed())
	{
		int selCount = 0;
		Vec3V total(V_ZERO);
		atMap<unsigned, EditorObject*>::Iterator iterator= m_SelectedObjects.CreateIterator();
		for(iterator.Start(); !iterator.AtEnd(); iterator.Next())
		{
			//If you selected an aggregate its owned volumes will be in the list
			if (!iterator.GetData()->m_Volume->IsAggregateVolume())
			{
				selCount++;
				iterator.GetData()->m_Volume->GetTransform(matrix);
				total = total + matrix.GetCol3();
			}
		}

		Vec3V center(V_ZERO);
		if (selCount > 1)
		{
			//Override the position since we have multiple selected non-aggregate volumes.
			center = (total / ScalarV((float)selCount));
			matrix.SetIdentity3x3();
			matrix.SetCol3(center);
		}

		if (m_TransWorldAligned && m_EditorWidget.InTranslateMode())
		{
			matrix.SetIdentity3x3();
		}
	}    
	m_EditorWidget.Update(matrix);
	//////////////////////////////////////////////////////////////////////////
}

void CVolumeEditor::Render()
{
	SYS_CS_SYNC(m_criticalSection);
	//Draw the aggregates list on screen
	m_isHoveredOnText = false;

#if DR_ENABLED
	int mousex = ioMouse::GetX();
	int mouseY = ioMouse::GetY();

	debugPlayback::OnScreenOutput volumeSpew(10.0f);
	volumeSpew.m_fXPosition = 50.0f;
	volumeSpew.m_fYPosition = 50.0f;
	volumeSpew.m_fMouseX = (float)mousex;
	volumeSpew.m_fMouseY = (float)mouseY;
	volumeSpew.m_Color.Set(160,160,160,255);
	volumeSpew.m_HighlightColor.Set(255,0,0,255);

	Assert(m_ScriptHandler);
	volumeSpew.m_bDrawBackground = true;
	volumeSpew.AddLine("%s", m_ScriptHandler->GetScriptName());
	volumeSpew.m_bDrawBackground = false;
	volumeSpew.AddLine("-------");
	volumeSpew.AddLine("Volumes");
	volumeSpew.AddLine("-------");

	debugPlayback::OnScreenOutput aggSpew(10.0f);
	aggSpew.m_fXPosition = 150.0f;
	aggSpew.m_fYPosition = 60.0f;
	aggSpew.m_fMouseX = (float)mousex;
	aggSpew.m_fMouseY = (float)mouseY;
	aggSpew.m_Color.Set(160,160,160,255);
	aggSpew.m_HighlightColor.Set(255,0,0,255);

	aggSpew.AddLine("------------");
	aggSpew.AddLine("Aggregates");
	aggSpew.AddLine("------------");
#endif

	//Hover sorting ... 
	grcViewport* viewport = grcViewport::GetCurrent();
	Vec3V startPoint;
	Vec3V endPoint;            
	viewport->ReverseTransform((float)ioMouse::GetX(), (float)ioMouse::GetY(), startPoint, endPoint);

	atBinaryMap<EditorObject*, float> hoverObjects;
	EditorObject* hover = NULL;
	EditorObject* current = m_EditorObjectsHead;
	while (current)
	{
		bool selected = (m_SelectedObjects.Access((unsigned)(current->m_Volume)))? true : false;

		if (!current->m_Volume->IsAggregateVolume())
		{
			Assert(current->m_Volume);
			CVolumePrimitiveBase* volprimbase = (CVolumePrimitiveBase*)current->m_Volume;
			Vec3V center;
			ScalarV radius;
			volprimbase->GetBoundSphere(center, radius);
			Vec4V sphere(center, radius);
			if (viewport->IsSphereVisible(sphere)) //visibility check ... 
			{
				//////////////////////////////////////////////////////////////////////////
				// Hover check
				Vec3V hitPoint(V_ZERO);
				if (current->m_Volume->DoesRayIntersect(startPoint, endPoint, hitPoint))
				{
					ScalarV dist = DistSquared(startPoint, hitPoint);
					hoverObjects.SafeInsert(dist.Getf(), current);
				}
				//////////////////////////////////////////////////////////////////////////


				//////////////////////////////////////////////////////////////////////////
				// Volume draw
				g_VolumeTool.SetRenderStyle(CVolumeTool::ersNormal);
				if (selected)
				{
					g_VolumeTool.SetRenderStyle(CVolumeTool::ersSelected);
				}
				else if (m_HoveredObject)
				{
					CVolumeAggregate* curowner = current->m_Volume->GetOwner();
					if (m_HoveredObject == current)
					{
						//If hovering on this object highlight it 
						g_VolumeTool.SetRenderStyle(CVolumeTool::ersHighlight);
					}
					else if (m_HoveredObject->m_Volume == curowner)
					{
						//If hovering on an object that is my aggregate highlight me
						g_VolumeTool.SetRenderStyle(CVolumeTool::ersHighlight);
					}
				}
				CVolumeManager::RenderDebugCB(current->m_Volume, NULL);
				//////////////////////////////////////////////////////////////////////////
			}
		}

		//////////////////////////////////////////////////////////////////////////
		// Text Draw
		if (current->m_Volume->IsAggregateVolume())
		{   
#if DR_ENABLED
			CVolumeAggregate* aggregate = (CVolumeAggregate*) current->m_Volume;
			if (selected || (m_HoveredObject && m_HoveredObject->m_Volume->GetOwner() == current->m_Volume))
			{
				aggSpew.m_bDrawBackground = true;
			}

			if (aggSpew.AddLine("%x - {%d}", (unsigned)current->m_Volume, aggregate->GetOwnedCount()))
			{
				hover = current;
				m_isHoveredOnText = true;
			}
			aggSpew.m_bDrawBackground = false;
#endif
		}
		else
		{
#if DR_ENABLED
			if (selected || 
				(m_HoveredObject && m_HoveredObject->m_Volume == current->m_Volume) ||
				(m_HoveredObject && m_HoveredObject->m_Volume == current->m_Volume->GetOwner())
				)
			{
				volumeSpew.m_bDrawBackground = true;
			}

			if (volumeSpew.AddLine("%x", (unsigned)current->m_Volume))
			{
				hover = current;
				m_isHoveredOnText = true;
			}
			volumeSpew.m_bDrawBackground = false;
#endif
		}
		//////////////////////////////////////////////////////////////////////////

		current = current->m_Next;
	}
	hoverObjects.FinishInsertion();

	//Figure out what is hovered on ... 
	if (hoverObjects.GetCount())
	{
		//the First one should be the closest ... 
		m_HoveredObject = *hoverObjects.GetItem(0);
#if DR_ENABLED
		if (m_ShowNameOfHovered)
		{
			debugPlayback::OnScreenOutput hoverSpew(10.0f);
			hoverSpew.m_fXPosition = (float)mousex + 15;
			hoverSpew.m_fYPosition = (float)mouseY;
			hoverSpew.m_fMouseX = (float)mousex;
			hoverSpew.m_fMouseY = (float)mouseY;
			hoverSpew.m_Color.Set(255,255,255,255);
			hoverSpew.m_HighlightColor.Set(255,0,0,255);

			hoverSpew.AddLine("%x", (unsigned)m_HoveredObject->m_Volume);
		}
#endif
	}
	else if (m_isHoveredOnText)
	{
		m_HoveredObject = hover;
	}
	else
	{
		m_HoveredObject = NULL;
	}

	// Widget draw...
	if (m_SelectedObjects.GetNumUsed())
	{
		m_EditorWidget.Render();
	}
	m_EditorWidget.RenderOptions();
}

void CVolumeEditor::CreateWidgets (bkBank* pBank)
{
	Assert(pBank);

	pBank->PushGroup("Creation", true);
#if __DEV
	pBank->AddButton("Create Box", &CVolumeEditor::CreateBox);
	pBank->AddButton("Create Sphere", &CVolumeEditor::CreateSphere);
	pBank->AddButton("Create Cylinder", &CVolumeEditor::CreateCylinder);
	pBank->AddButton("Create Aggregate", &CVolumeEditor::CreateAggregate);
#endif // __DEV
	pBank->PopGroup();
	pBank->AddButton("Add Selected To New Aggregate", &CVolumeEditor::CreateAggregateWithSelected);
	pBank->AddButton("Add Selected To Aggregate", &CVolumeEditor::MergeSelectedIntoAggregate);
	pBank->AddButton("Remove Selected from Aggregates", &CVolumeEditor::ClearSelectedFromAggregates);

	pBank->AddSlider("Translation Step", &m_transStep, 0.01f, 2.0f, 0.01f );
	pBank->AddSlider("Scale Step", &m_scaleStep, 0.01f, 2.0f, 0.01f );
	pBank->AddAngle("Rotation Step", &m_rotateStep, bkAngleType::DEGREES, 0.0f, 360.0f);
	pBank->AddToggle("Perform Local Rotations", &m_LocalRotations);
	pBank->AddToggle("Translation World Aligned", &m_TransWorldAligned);
	pBank->AddToggle("Snap Volumes To Ground", &m_SnapVolumes, datCallback(CFA1(CVolumeEditor::OnVolumeSnappingChangedCB), (void *) this));
	pBank->AddToggle("Show name of Hovered", &m_ShowNameOfHovered);
	pBank->AddButton("Delete Selected", &CVolumeEditor::DeleteSelectedObjects);
}

void CVolumeEditor::DeleteSelected ()
{
	atMap<unsigned, EditorObject*>::Iterator iterator= m_SelectedObjects.CreateIterator();
	for(iterator.Start(); !iterator.AtEnd(); iterator.Next())
	{
		EditorObject* toDelete = iterator.GetData();
		//Isolate the node
		if (toDelete->m_Next)
		{
			toDelete->m_Next->m_Prev = toDelete->m_Prev;
		}

		if (toDelete->m_Prev)
		{
			toDelete->m_Prev->m_Next = toDelete->m_Next;
		}

		if (m_EditorObjectsHead == toDelete)
		{
			Assertf(!toDelete->m_Prev, "Somehow the head node has a non-NULL m_Prev pointer.");
			m_EditorObjectsHead = toDelete->m_Next;
		}

		//remove myself from any aggregates i belong too
		CVolumeAggregate* owner = toDelete->m_Volume->GetOwner();
		if (owner)
		{
			owner->Remove(toDelete->m_Volume);
		}

		//If i am an aggregate ... empty my linked list array
		if (toDelete->m_Volume->IsAggregateVolume())
		{
			CVolumeAggregate* me = (CVolumeAggregate*)toDelete->m_Volume;
			me->RemoveAll();
		}

		//Do the delete
		int VolumeGUID = fwScriptGuid::GetGuidFromBase((*(toDelete->m_Volume)));
		Assertf(m_ScriptHandler, "CVolumeEditor::DeleteSelected m_ScriptHandler is somehow invalid!!");
		m_ScriptHandler->RemoveScriptResource(CGameScriptResource::SCRIPT_RESOURCE_VOLUME, VolumeGUID);

		//clear to delete
		delete toDelete;
	}

	ClearSelected();
}

void CVolumeEditor::ClearSelected ()
{
	m_SelectedObjects.Reset();//nothing selected anymore
	m_EditorWidget.Reset();//be safe we dont lock selection
}

void CVolumeEditor::AddSelectedToAggregate (CVolume* aggregate/*=NULL*/)
{
	if (m_SelectedObjects.GetNumUsed())
	{
		CVolumeAggregate* pVolume = (CVolumeAggregate*)aggregate;
		if (!pVolume)
		{
			pVolume = (CVolumeAggregate*)g_VolumeManager.CreateVolumeAggregate();
			AddEditable(pVolume, true); //Add to editor
		}

		atMap<unsigned, EditorObject*>::Iterator iterator = m_SelectedObjects.CreateIterator();
		for(iterator.Start(); !iterator.AtEnd(); iterator.Next())
		{
			if (iterator.GetData()->m_Volume->GetOwner() != pVolume &&
				iterator.GetData()->m_Volume != pVolume //no agg vol can be its owner
			   )
			{
				pVolume->Add(iterator.GetData()->m_Volume);
			}
		}
	}
}

void CVolumeEditor::RemoveSelectedFromAggregates ()
{
	atMap<unsigned, EditorObject*>::Iterator iterator = m_SelectedObjects.CreateIterator();
	for(iterator.Start(); !iterator.AtEnd(); iterator.Next())
	{
		CVolume* volume = iterator.GetData()->m_Volume;
		CVolumeAggregate* owner = volume->GetOwner();
		if (owner)
		{
			owner->Remove(volume);
		}
	}
}

void CVolumeEditor::AddSelectedToSelectedAggregate ()
{
	CVolumeAggregate* metaowner = NULL;
	//Find the aggregate that is selected ... (should be unique)
	// and will be from one of selected objects
	atMap<unsigned, EditorObject*>::Iterator iterator = m_SelectedObjects.CreateIterator();
	for(iterator.Start(); !iterator.AtEnd(); iterator.Next())
	{
		CVolume* volume = iterator.GetData()->m_Volume;
		CVolumeAggregate* owner = volume->GetOwner();
		if (volume->IsAggregateVolume() && !owner)
		{
			if(!metaowner)
			{
				metaowner = (CVolumeAggregate*)volume;
			}
			else if (metaowner != (CVolumeAggregate*)volume)
			{
				metaowner = NULL;
				Errorf("Multiple aggregates are represented in the selected list please only select one or create a new aggregate from the selected volumes.");
				break;
			}
		}
		else if (!volume->IsAggregateVolume() && owner)
		{
			if(!metaowner)
			{
				metaowner = owner;
			}
			else if (metaowner != owner)
			{
				metaowner = NULL;
				Errorf("Multiple aggregates are represented in the selected list please only select one or create a new aggregate from the selected volumes.");
				break;
			}
		}
		else if (volume->IsAggregateVolume() && owner) //aggregates of aggregates ... not supported at moment ...
		{
			metaowner = NULL;
			Errorf("Multiple aggregates are represented in the selected list please only select one or create a new aggregate from the selected volumes.");
			break;
		}
	}

	if (!metaowner)
	{
		Errorf("No aggregate selected please select an volume in the aggregate you would like to append too.");
	}
	else
	{
		//For each object add it to the found aggregate ... this function will ignore any selected volumes
		// already in the aggregate
		AddSelectedToAggregate(metaowner);
	}
}

void CVolumeEditor::AddEditable (CVolume* volume, bool toolCreated/*=false*/)
{
	//TODO: Perhaps a lookup check here to make sure we have not already added this object to the editor
	EditorObject* newEO = rage_new EditorObject();
	if (m_EditorObjectsHead)
	{
		m_EditorObjectsHead->m_Prev = newEO;
	}
	newEO->m_Next = m_EditorObjectsHead;
	m_EditorObjectsHead = newEO;
	newEO->m_Volume = volume;

	//Tool created items need to be added to the script handler
	if (toolCreated)
	{
		Assertf(m_ScriptHandler, "CVolumeEditor::AddEditable Somehow m_ScriptHandler is invalid.");
		CScriptResource_Volume newVS(newEO->m_Volume);
		m_ScriptHandler->RegisterScriptResourceAndGetRef(newVS);
	}
}

void CVolumeEditor::GetSelectedList (atMap<unsigned, const CVolume*>& toFill) const
{
	atMap<unsigned, EditorObject*>::ConstIterator iterator = m_SelectedObjects.CreateIterator();
	for(iterator.Start(); !iterator.AtEnd(); iterator.Next())
	{
		toFill.Insert((unsigned)iterator.GetData()->m_Volume, iterator.GetData()->m_Volume);
	}
}

Vec3V_Out CVolumeEditor::GetSnappedWorldPosition(Vec3V_In worldPos, ScalarV_In probeOffset)
{
	Vec3V upVec = Vec3V(V_Z_AXIS_WZERO) * probeOffset;
	Vec3V downVec = -upVec;
	WorldProbe::CShapeTestProbeDesc probeDesc;
	WorldProbe::CShapeTestResults probeResult;
	probeDesc.SetResultsStructure(&probeResult);
	probeDesc.SetStartAndEnd(VEC3V_TO_VECTOR3(worldPos + upVec), VEC3V_TO_VECTOR3(worldPos + downVec));
	probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);
	Vec3V adjustedPos = worldPos;
	if(Likely(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc)))
	{
		adjustedPos = probeResult[0].GetHitPositionV();
	}
	return adjustedPos;
}

void CVolumeEditor::OnVolumeSnappingChanged()
{
	// make sure the currently selected volume is snapped
	// otherwise the snap only takes effect when translating the 
	// volume for the first time after enabling the setting
	if (m_SnapVolumes)
	{
		atMap<unsigned, EditorObject*>::Iterator iterator = m_SelectedObjects.CreateIterator();
		Vec3V curPos;
		for(iterator.Start(); !iterator.AtEnd(); iterator.Next())
		{
			//Aggregates dont move they contain volumes ... 
			if (iterator.GetData()->m_Volume->IsAggregateVolume())
				continue;
			if (Likely(iterator.GetData()->m_Volume->GetPosition(curPos)))
			{
				Vec3V adjustedPos = GetSnappedWorldPosition(curPos, ScalarV(WORLDLIMITS_ZMAX));
				iterator.GetData()->m_Volume->SetPosition(adjustedPos);
			}
		}
	}
}

void CVolumeEditor::OnEditorWidgetEvent (const CEditorWidget::EditorWidgetEvent& _event)
{
	ScalarV scaleStep(m_scaleStep);
	ScalarV transStep(m_transStep);
	ScalarV rotateStep((m_rotateStep*DtoR));//convert to radians

	//Perform commands to selected
	bool multiSelect = (m_SelectedObjects.GetNumUsed() > 1) ? true : false;
	atMap<unsigned, EditorObject*>::Iterator iterator = m_SelectedObjects.CreateIterator();
	for(iterator.Start(); !iterator.AtEnd(); iterator.Next())
	{
		//Aggregates dont move they contain volumes ... 
		if (iterator.GetData()->m_Volume->IsAggregateVolume())
			continue;

		if (_event.IsRotate())
		{
			Mat33V rX(V_IDENTITY);
			Mat33V rY(V_IDENTITY);
			Mat33V rZ(V_IDENTITY);
			if (_event.RotateX())
			{
				Mat33VFromXAxisAngle(rX, rotateStep * _event.GetDelta());
			}
			if (_event.RotateY())
			{
				Mat33VFromYAxisAngle(rY, rotateStep * _event.GetDelta());
			}
			if (_event.RotateZ())
			{
				Mat33VFromZAxisAngle(rZ, rotateStep * _event.GetDelta());
			}
			Mat33V final(V_IDENTITY);
			Multiply(final, rX, rY);
			Multiply(final, final, rZ);
			if (multiSelect && !m_LocalRotations)
			{
				iterator.GetData()->m_Volume->RotateAboutPoint(final, m_EditorWidget.GetPosition());
			}
			else
			{
				iterator.GetData()->m_Volume->Rotate(final);
			}

		}

		if (_event.IsScale())
		{
			Vec3V scale(V_ZERO);
			if (_event.ScaleX())
			{
				scale.SetX(scaleStep * _event.GetDelta());
			}
			if (_event.ScaleY())
			{
				scale.SetY(scaleStep * _event.GetDelta());
			}
			if (_event.ScaleZ())
			{
				scale.SetZ(scaleStep * _event.GetDelta());
			}
			iterator.GetData()->m_Volume->Scale(scale);
		}

		if (_event.IsTranslate())
		{
			Vec3V trans(V_ZERO);
			Mat34V mat;
			Vec3V vec(V_ZERO);
			iterator.GetData()->m_Volume->GetTransform(mat);
			if (_event.TranslateX())
			{   
				if (m_TransWorldAligned)
				{
					vec = Vec3V(V_X_AXIS_WZERO); //world Z
				}
				else
				{
					vec = mat.GetCol0();
				}
				trans = trans + vec * (transStep * _event.GetDelta());
			}
			if (_event.TranslateY())
			{
				if (m_TransWorldAligned)
				{
					vec = Vec3V(V_Y_AXIS_WZERO); //world Y
				}
				else
				{
					vec = mat.GetCol1();
				}
				trans = trans + vec * (transStep * _event.GetDelta());
			}
			if (_event.TranslateZ())
			{
				if (m_TransWorldAligned)
				{
					vec = Vec3V(V_Z_AXIS_WZERO); //world Z
				}
				else
				{
					vec = mat.GetCol2();
				}
				trans = trans + vec * (transStep * _event.GetDelta());
			}
			if (m_SnapVolumes)
			{
				// if snapping is enabled, allow translation, but force Z axis values to be the ground position
				trans = GetSnappedWorldPosition(mat.GetCol3() + trans, ScalarV(WORLDLIMITS_ZMAX)) - mat.GetCol3();
			}
			iterator.GetData()->m_Volume->Translate(trans);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
//  STATIC CODE
///////////////////////////////////////////////////////////////////////////////
#if __DEV
void CVolumeEditor::CreateBox()
{
	Mat34V orient(V_IDENTITY);
	Vec3V scale(V_ONE);

	// generate a location to create the object from the camera position & orientation
	Vector3 srcPos = camInterface::GetPos();
	Vector3 destPos = srcPos;
	Vector3 viewVector = camInterface::GetFront();

	// create at position DIST_IN_FRONT meters in front of current camera position
	static dev_float DIST_IN_FRONT = 7.0f;
	destPos += viewVector*DIST_IN_FRONT; 
	Vec3V position = VECTOR3_TO_VEC3V(destPos);

	CVolume* pVolume = g_VolumeManager.CreateVolumeBox(position, orient.GetMat33(), scale);
	g_VolumeTool.GetEditor().AddEditable(pVolume, true);
}

void CVolumeEditor::CreateSphere()
{
	Mat34V orient(V_IDENTITY);
	Vec3V scale(V_ONE);

	// generate a location to create the object from the camera position & orientation
	Vector3 srcPos = camInterface::GetPos();
	Vector3 destPos = srcPos;
	Vector3 viewVector = camInterface::GetFront();

	// create at position DIST_IN_FRONT meters in front of current camera position
	static dev_float DIST_IN_FRONT = 7.0f;
	destPos += viewVector*DIST_IN_FRONT; 
	Vec3V position = VECTOR3_TO_VEC3V(destPos);

	CVolume* pVolume = g_VolumeManager.CreateVolumeSphere(position, orient.GetMat33(), scale);
	g_VolumeTool.GetEditor().AddEditable(pVolume, true);
}

void CVolumeEditor::CreateCylinder()
{
	Mat34V orient(V_IDENTITY);
	Vec3V scale(V_ONE);

	// generate a location to create the object from the camera position & orientation
	Vector3 srcPos = camInterface::GetPos();
	Vector3 destPos = srcPos;
	Vector3 viewVector = camInterface::GetFront();

	// create at position DIST_IN_FRONT meters in front of current camera position
	static dev_float DIST_IN_FRONT = 7.0f;
	destPos += viewVector*DIST_IN_FRONT; 
	Vec3V position = VECTOR3_TO_VEC3V(destPos);

	CVolume* pVolume = g_VolumeManager.CreateVolumeCylinder(position, orient.GetMat33(), scale);
	g_VolumeTool.GetEditor().AddEditable(pVolume, true);
}

void CVolumeEditor::CreateAggregate()
{
	CVolume* pVolume = g_VolumeManager.CreateVolumeAggregate();
	g_VolumeTool.GetEditor().AddEditable(pVolume, true); //Add to editor
}
#endif // __DEV

void CVolumeEditor::DeleteSelectedObjects()
{
	g_VolumeTool.GetEditor().DeleteSelected();
}

void CVolumeEditor::CreateAggregateWithSelected()
{
	g_VolumeTool.GetEditor().AddSelectedToAggregate();
}

void CVolumeEditor::ClearSelectedFromAggregates()
{
	g_VolumeTool.GetEditor().RemoveSelectedFromAggregates();
}

void CVolumeEditor::MergeSelectedIntoAggregate()
{
	g_VolumeTool.GetEditor().AddSelectedToSelectedAggregate();
}

bool CVolumeEditor::AddScriptResourceToEditorCB (const scriptResource* resource, void* data)
{
	if (resource->GetType() == CGameScriptResource::SCRIPT_RESOURCE_VOLUME)
	{
		CVolumeEditor* editor = (CVolumeEditor*)data;
		CVolume *pVolume = fwScriptGuid::FromGuid<CVolume>(resource->GetReference());
		editor->AddEditable(pVolume);
	}
	return true;
}

void CVolumeEditor::OnVolumeSnappingChangedCB(void *obj)
{
	Assert(obj);
	static_cast<CVolumeEditor *>(obj)->OnVolumeSnappingChanged();
}

#endif // __BANK && VOLUME_SUPPORT