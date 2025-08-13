<?xml version="1.0"?> 
<ParserSchema xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
							xsi:noNamespaceSchemaLocation="schemas/parsermetadata.xsd">

  <structdef type="CAnimScenePlayVfxEvent" base="CAnimSceneEvent" onPostAddWidgets="OnPostAddWidgets">
    <string name="m_vfxName" type="atHashString" description="The name of the particle effect to play"/>
    <struct name="m_entity" type="CAnimSceneEntityHandle" description="Entity to play back the particle effect on" />
    <Vec3V name="m_offsetPosition" description="The offset from the entity (and bone if specified) to play back the effect"/>
    <Vec3V name="m_offsetEulerRotation" description="The euler rotation offset from the entity (and bone) to play back the effect"/>
    <float name="m_scale" init="1.0f" min="0.0f" max="100000000.0f" description="The scale of the effect" />
    <int name="m_probability" init="100" min="0" max="100" description="The likelihood of the effect actually triggering (integer value from 0-100%)" />
    <enum	name="m_boneId" type="eAnimBoneTag" description="The bone on the entity to attach the effect"/>
    <Color32 	name="m_color" description="The tint color for the effect"/>
    <bool name="m_continuous" description="True if the effect is a continuous effect, false if it's a one off trigger (only activated on the event enter)" />
    <bool name="m_useEventTags" description="If true, the anim scene will load the event, but will only trigger / update it when it detects an appropriate vfx event tag from animations playing on the entity" />
    <pad bytes="4" platform="32bit"/>    <!-- m_data -->
    <pad bytes="8" platform="64bit"/>
  </structdef>

</ParserSchema>