<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

	<structdef type="CScenarioEntityOverride" simple="true">
		<Vec3V name="m_EntityPosition"/>
		<string name="m_EntityType" type="atHashString"/>
		<array name="m_ScenarioPoints" type="atArray">
			<struct type="CExtensionDefSpawnPoint"/>
		</array>
		<pad bytes="12" platform="32bit"/><!-- 1x atArray, 1x RegdEnt -->
		<pad bytes="24" platform="64bit"/>
		<bool name="m_EntityMayNotAlwaysExist" init="false"/>
		<bool name="m_SpecificallyPreventArtPoints" init="false"/>
		<pad bytes="1"/><!-- u8 m_VerificationStatus __BANK functionality -->
	</structdef>

	<structdef type="CScenarioPointLookUps" simple="true">
		<array name="m_TypeNames" type="atArray">
			<string type="atHashString"/>
		</array>
		<array name="m_PedModelSetNames" type="atArray">
			<string type="atHashString"/>
		</array>
		<array name="m_VehicleModelSetNames" type="atArray">
			<string type="atHashString"/>
		</array>
		<array name="m_GroupNames" type="atArray">
			<string type="atHashString"/>
		</array>
		<array name="m_InteriorNames" type="atArray">
			<string type="atHashString"/>
		</array>
		<array name="m_RequiredIMapNames" type="atArray">
			<string type="atHashString"/>
		</array>
	</structdef>

	<structdef type="CScenarioPointRegion" onPostLoad="ParserPostLoad" simple="true">
		<int name="m_VersionNumber" type="int" init="0"/>
		<struct name="m_Points" type="CScenarioPointContainer"/>
	<pad bytes="8" platform="32bit"/><!-- atArray<CScenarioEntityOverride> m_EntityOverrides -->
	<pad bytes="16" platform="64bit"/><!-- atArray<CScenarioEntityOverride> m_EntityOverrides -->
		<array name="m_EntityOverrides" type="atArray">
			<struct type="CScenarioEntityOverride"/>
		</array>
	<pad bytes="1"/><!-- bool m_bContainsBuildings and m_bContainsObjects -->
		<struct name="m_ChainingGraph" type="CScenarioChainingGraph"/>
		<struct name="m_AccelGrid" type="rage::spdGrid2D"/>
		<array name="m_AccelGridNodeIndices" type="atArray">
			<u16/>
		</array>
		<array name="m_Clusters" type="atArray">
			<struct type="CScenarioPointCluster"/>
		</array>
		<struct name="m_LookUps" type="CScenarioPointLookUps"/>
	</structdef>

	<structdef type="CScenarioPointCluster" simple="true">
		<struct name="m_Points" type="CScenarioPointContainer"/>
		<struct name="m_ClusterSphere" type="rage::spdSphere"/>
		<float name="m_fNextSpawnAttemptDelay" init="30.0f"/>
		<bool name="m_bAllPointRequiredForSpawn" init="false"/>
	</structdef>

	<structdef type="CScenarioPoint" simple="true"><!-- base="fwRefAwareBaseNonVirt"> -->
		<pad bytes="4" platform="32bit"/><!-- fwRefAwareBase::fwKnownRefHolder* -->
		<pad bytes="8" platform="64bit"/><!-- fwRefAwareBase::fwKnownRefHolder* -->
		<pad bytes="4" platform="32bit"/><!-- m_pEntity -->
		<pad bytes="8" platform="64bit"/><!-- m_pEntity x64 size-->
		<pad bytes="2"/><!-- m_RtFlags m_iUses m_uInteriorState -->
		<pad bytes="2"/><!-- m_CarGenId #if __BANK m_bEditable #endif // __BANK -->
		<pad bytes="1"/><!-- m_iFailedCollisionCheckTimeCount -->
		<u8 name="m_iType"/>
		<u8 name="m_ModelSetId"/>
		<u8 name="m_iInterior"/>
		<u8 name="m_iRequiredIMapId"/>
		<u8 name="m_iProbability"/>
		<u8 name="m_uAvailableInMpSp"/>
		<u8 name="m_iTimeStartOverride"/>
		<u8 name="m_iTimeEndOverride"/>
		<u8 name="m_iRadius"/>
		<u8 name="m_iTimeTillPedLeaves"/>
		<u16 name="m_iScenarioGroup"/>
		<bitset	name="m_Flags" type="fixed32" values="CScenarioPointFlags::Flags"/>
		<Vec4V name="m_vPositionAndDirection"/>
	</structdef>

	<structdef type="CScenarioPointContainer" simple="true">
		<array name="m_LoadSavePoints" type="atArray">
			<struct type="CExtensionDefSpawnPoint"/>
		</array>
		<array name="m_MyPoints" type="atArray">
			<struct type="CScenarioPoint"/>
		</array>
		<pad bytes="8" platform="32bit"/><!-- 1x atArray m_EditingPoints -->
		<pad bytes="16" platform="64bit"/><!-- 1x atArray m_EditingPoints -->
	</structdef>

	<structdef type="CScenarioPointManifest" simple="true">
		<int name="m_VersionNumber" type="int" init="0"/>
		<array name="m_RegionDefs" type="atArray">
			<pointer type="CScenarioPointRegionDef" policy="owner"/>
		</array>
		<array name="m_Groups" type="atArray">
			<pointer type="CScenarioPointGroup" policy="owner"/>
		</array>
		<array name="m_InteriorNames" type="atArray">
			<string type="atHashString"/>
		</array>
	</structdef>

	<structdef type="CScenarioPointRegionDef" simple="true">
		<string name="m_Name" type="atFinalHashString"/>
		<struct name="m_AABB" type="rage::spdAABB"/>
		<pad bytes="4"/><!-- 1x int m_SlotId -->
	</structdef>

	<structdef type="CScenarioChainingNode" simple="true">
		<Vec3V name="m_Position"/>
		<string name="m_AttachedToEntityType" type="atHashString"/>
		<string name="m_ScenarioType" type="atHashString"/>
		<bool name="m_HasIncomingEdges" init="false"/>
		<bool name="m_HasOutgoingEdges" init="false"/>
	</structdef>

	<enumdef type="CScenarioChainingEdge::eAction">
		<enumval name="Move"/>
		<enumval name="MoveIntoVehicleAsPassenger"/>
		<enumval name="MoveFollowMaster"/>
	</enumdef>

	<enumdef type="CScenarioChainingEdge::eNavMode">
		<enumval name="Direct"/>
		<enumval name="NavMesh"/>
		<enumval name="Roads"/>
	</enumdef>

	<enumdef type="CScenarioChainingEdge::eNavSpeed">
		<enumval name="kSpd5mph"/>
		<enumval name="kSpd10mph"/>
		<enumval name="kSpd15mph"/>
		<enumval name="kSpd25mph"/>
		<enumval name="kSpd35mph"/>
		<enumval name="kSpd45mph"/>
		<enumval name="kSpd55mph"/>
		<enumval name="kSpd65mph"/>
		<enumval name="kSpd80mph"/>
		<enumval name="kSpd100mph"/>
		<enumval name="kSpd125mph"/>
		<enumval name="kSpd150mph"/>
		<enumval name="kSpd200mph"/>
		<enumval name="kSpdWalk"/>
		<enumval name="kSpdRun"/>
		<enumval name="kSpdSprint"/>
	</enumdef>

	<structdef type="CScenarioChainingEdge" simple="true">
		<u16 name="m_NodeIndexFrom"/>
		<u16 name="m_NodeIndexTo"/>
		<enum name="m_Action" type="CScenarioChainingEdge::eAction" size="8" init="0"/>
		<enum name="m_NavMode" type="CScenarioChainingEdge::eNavMode" size="8" init="CScenarioChainingEdge::kNavModeDefault"/>
		<enum name="m_NavSpeed" type="CScenarioChainingEdge::eNavSpeed" size="8" init="CScenarioChainingEdge::kNavSpeedDefault"/>
	</structdef>

	<structdef type="CScenarioChain" simple="true">
		<u8 name="m_MaxUsers" init="1" min="1" max="255"/>
		<array name="m_EdgeIds" type="atArray">
			<u16/>
		</array>
		<pad bytes="8" platform="32bit"/><!-- 1x atArray m_Users -->
		<pad bytes="16" platform="64bit"/>
	</structdef>

	<structdef type="CScenarioChainingGraph" simple="true">
		<array name="m_Nodes" type="atArray">
			<struct type="CScenarioChainingNode"/>
		</array>
		<array name="m_Edges" type="atArray">
			<struct type="CScenarioChainingEdge"/>
		</array>
		<array name="m_Chains" type="atArray">
			<struct type="CScenarioChain"/>
		</array>
		<pad bytes="17" platform="32bit"/><!-- 2x atArrays, 1x bool -->
		<pad bytes="33" platform="64bit"/><!-- 2x atArrays, 1x bool -->
	</structdef>

	<structdef type="CScenarioPointGroup" simple="true">
		<string name="m_Name" type="atHashString"/>
		<bool name="m_EnabledByDefault" init="false"/>
	</structdef>

	<enumdef type="CSpawnPoint::AvailabilityMpSp">
		<enumval name="kBoth"/>
		<enumval name="kOnlySp"/>
		<enumval name="kOnlyMp"/>
	</enumdef>

</ParserSchema>
