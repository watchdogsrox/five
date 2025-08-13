<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">
	
  <structdef type="CVehicleMetadataMgr" onPostLoad="ValidateData">
		<array name="m_AnimRateSets" type="atArray">
			<pointer type="CAnimRateSet" policy="owner"/>
		</array>
		<array name="m_ClipSetMaps" type="atArray">
			<pointer type="CClipSetMap" policy="owner"/>
		</array>
		<array name="m_VehicleCoverBoundOffsetInfos" type="atArray">
			<pointer type="CVehicleCoverBoundOffsetInfo" policy="owner"/>
		</array>
		<array name="m_BicycleInfos" type="atArray">
			<pointer type="CBicycleInfo" policy="owner"/>
		</array>
		<array name="m_POVTuningInfos" type="atArray">
			<pointer type="CPOVTuningInfo" policy="owner"/>
		</array>
		<array name="m_EntryAnimVariations" type="atArray">
			<pointer type="CEntryAnimVariations" policy="owner"/>
		</array>
		<array name="m_VehicleExtraPointsInfos" type="atArray">
			<pointer type="CVehicleExtraPointsInfo" policy="owner"/>
		</array>
		<array name="m_DrivebyWeaponGroups" type="atArray">
			<pointer type="CDrivebyWeaponGroup" policy="owner"/>
		</array>
		<array name="m_VehicleDriveByAnimInfos" type="atArray">
			<pointer type="CVehicleDriveByAnimInfo" policy="owner"/>
		</array>
		<array name="m_VehicleDriveByInfos" type="atArray">
			<pointer type="CVehicleDriveByInfo" policy="owner"/>
		</array>    
		<array name="m_VehicleSeatInfos" type="atArray">
			<pointer type="CVehicleSeatInfo" policy="owner"/>
		</array>
		<array name="m_VehicleSeatAnimInfos" type="atArray">
			<pointer type="CVehicleSeatAnimInfo" policy="owner"/>
		</array>
		<array name="m_VehicleEntryPointInfos" type="atArray">
			<pointer type="CVehicleEntryPointInfo" policy="owner"/>
		</array>
		<array name="m_VehicleEntryPointAnimInfos" type="atArray">
			<pointer type="CVehicleEntryPointAnimInfo" policy="owner"/>
		</array>
		<array name="m_VehicleExplosionInfos" type="atArray">
			<pointer type="CVehicleExplosionInfo" policy="owner"/>
		</array>
		<array name="m_VehicleLayoutInfos" type="atArray">
			<pointer type="CVehicleLayoutInfo" policy="owner"/>
		</array>
		<array name="m_VehicleScenarioLayoutInfos" type="atArray">
			<pointer type="CVehicleScenarioLayoutInfo" policy="owner"/>
		</array>
		<array name="m_SeatOverrideAnimInfos" type="atArray">
			<pointer type="CSeatOverrideAnimInfo" policy="owner"/>
		</array>
		<array name="m_InVehicleOverrideInfos" type="atArray">
			<pointer type="CInVehicleOverrideInfo" policy="owner"/>
		</array>
		<array name="m_FirstPersonDriveByLookAroundData" type="atArray">
			<pointer type="CFirstPersonDriveByLookAroundData" policy="owner"/>
		</array>
	</structdef>

</ParserSchema>