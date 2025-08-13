<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

<structdef type="framesPerSecondResult">
	<float name="min"/>
	<float name="max"/>
	<float name="average"/>
	<float name="std"/>
</structdef>

<structdef type="msPerFrameResult">
  <float name="min"/>
  <float name="max"/>
  <float name="average"/>
  <float name="std"/>
</structdef>

<structdef type="streamingMemoryCategoryResult">
	<string name="categoryName" type="atString"/>
	<u32 name="virtualMemory"/>
	<u32 name="physicalMemory"/>
</structdef>

<const name="CDataFileMgr::CONTENTS_MAX" value="10"/>

<structdef type="streamingMemoryResult">
	<string name="moduleName" type="atString"/>
	<array name="categories" type="member" size="CDataFileMgr::CONTENTS_MAX">
		<struct type="streamingMemoryCategoryResult"/>
	</array>
</structdef>

<structdef type="memoryDistributionResult">
	<u32 name="idx"/>
	<struct name="physicalUsedBySize" type="memoryUsageResult"/>
	<struct name="physicalFreeBySize" type="memoryUsageResult"/>
	<struct name="virtualUsedBySize" type="memoryUsageResult"/>
	<struct name="virtualFreeBySize" type="memoryUsageResult"/>
</structdef>

<structdef type="gpuTimingResult">
	<u32 name="idx"/>
	<string name="name" type="atString"/>
	<float name="time"/>
</structdef>

<structdef type="cpuTimingResult">
    <u32 name="idx"/>
    <string name="name" type="atString"/>
    <string name="set" type="atString"/>
    <float name="min"/>
    <float name="max"/>
    <float name="average"/>
	<float name="std"/>
</structdef>

<structdef type="threadTimingResult">
	<u32 name="idx"/>
	<string name="name" type="atString"/>
	<float name="min"/>
	<float name="max"/>
	<float name="average"/>
	<float name="std"/>
</structdef>
	
<structdef type="memoryHeapResult">
	<u32 name="idx"/>
   	<string name="name" type="atString"/>
    <size_t name="used"/>
	<size_t name="free"/>
	<size_t name="total"/>
	<size_t name="peak"/>
	<size_t name="fragmentation"/>
</structdef>

<structdef type="memoryUsageResult">
    <u32 name="min"/>
    <u32 name="max"/>
    <u32 name="average"/>
	<float name="std"/>
</structdef>

<structdef type="memoryBucketUsageResult">
    <u32 name="idx"/>
    <string name="name" type="atString"/>
    <struct name="gameVirtual" type="memoryUsageResult"/>
    <struct name="gamePhysical" type="memoryUsageResult"/>
    <struct name="resourceVirtual" type="memoryUsageResult"/>
    <struct name="resourcePhysical" type="memoryUsageResult"/>
</structdef>

<structdef type="scriptMemoryUsageResult">
	<struct name="physicalMem" type="memoryUsageResult"/>
	<struct name="virtualMem" type="memoryUsageResult"/>
</structdef>

<structdef type="streamingStatsResult">
	<string name="name" type="atString"/>

	<u32 name="avgLoaded"/>
	<u32 name="minLoaded"/>
	<u32 name="maxLoaded"/>
	<float name="stdLoaded"/>
	<u32 name="totalLoaded"/>

	<u32 name="avgRequested"/>
	<u32 name="minRequested"/>
	<u32 name="maxRequested"/>
	<float name="stdRequested"/>
	<u32 name="totalRequested"/>
</structdef>

<structdef type="drawListResult">
    <u32 name="idx"/>
    <string name="name" type="atString"/>
    <u32 name="min"/>
    <u32 name="max"/>
    <u32 name="average"/>
	<float name="std"/>
</structdef>

<structdef type="minMaxAverageResult">
	<u32 name="min"/>
	<u32 name="max"/>
	<u32 name="average"/>
</structdef>

<structdef type="pedAndVehicleStatResults">
	<struct name="numPeds" type="minMaxAverageResult"/>
	<struct name="numActivePeds" type="minMaxAverageResult"/>
	<struct name="numInactivePeds" type="minMaxAverageResult"/>
	<struct name="numEventScanHiPeds" type="minMaxAverageResult"/>
	<struct name="numEventScanLoPeds" type="minMaxAverageResult"/>
	<struct name="numMotionTaskHiPeds" type="minMaxAverageResult"/>
	<struct name="numMotionTaskLoPeds" type="minMaxAverageResult"/>
	<struct name="numPhysicsHiPeds" type="minMaxAverageResult"/>
	<struct name="numPhysicsLoPeds" type="minMaxAverageResult"/>
	<struct name="numEntityScanHiPeds" type="minMaxAverageResult"/>
	<struct name="numEntityScanLoPeds" type="minMaxAverageResult"/>
	<struct name="numVehicles" type="minMaxAverageResult"/>
	<struct name="numActiveVehicles" type="minMaxAverageResult"/>
	<struct name="numInactiveVehicles" type="minMaxAverageResult"/>
	<struct name="numRealVehicles" type="minMaxAverageResult"/>
	<struct name="numDummyVehicles" type="minMaxAverageResult"/>
	<struct name="numSuperDummyVehicles" type="minMaxAverageResult"/>
</structdef>
	
<structdef type="streamingModuleResult">
	<u32 name="moduleId"/>
	<string name="moduleName" type="atString"/>
	<size_t name="loadedMemVirt"/>
	<size_t name="loadedMemPhys"/>
	<size_t name="requiredMemVirt"/>
	<size_t name="requiredMemPhys"/>
</structdef>

<structdef type="metricsList">
	<u32 name="idx"/>
	<string name="name" type="atString"/>
	<pointer name="fpsResult" type="framesPerSecondResult" policy="owner"/>
	<pointer name="msPFResult" type="msPerFrameResult" policy="owner"/> 
	<array name="gpuResults" type="atArray">
		<pointer type="gpuTimingResult" policy="owner"/>
	</array>
	<array name="cpuResults" type="atArray">
	<pointer type="cpuTimingResult" policy="owner"/>
	</array>
	<array name="threadResults" type="atArray">
	<pointer type="threadTimingResult" policy="owner"/>
	</array>
    <array name="memoryResults" type="atArray">
        <pointer type="memoryBucketUsageResult" policy="owner"/>
    </array>
	<array name="memoryHeapResults" type="atArray">
        <pointer type="memoryHeapResult" policy="owner"/>
    </array>
	<array name="streamingMemoryResults" type="atArray">
	    <pointer type="streamingMemoryResult" policy="owner"/>
	</array>
	<array name="streamingStatsResults" type="atArray">
		<pointer type="streamingStatsResult" policy="owner"/>
	</array>
	<array name="streamingModuleResults" type="atArray">
		<pointer type="streamingModuleResult" policy="owner"/>
	</array>
    <array name="drawListResults" type="atArray">
        <pointer type="drawListResult" policy="owner"/>
    </array>
	<array name="memDistributionResults" type="atArray">
		<pointer type="memoryDistributionResult" policy="owner"/>
	</array>
	<pointer name="scriptMemResult" type="scriptMemoryUsageResult" policy="owner"/>
	<pointer name="pedAndVehicleResults" type="pedAndVehicleStatResults" policy="owner"/>
</structdef>

<structdef type="debugLocationMetricsList">
	<string name="platform" type="atString"/>
	<string name="buildversion" type="atString"/>
	<string name="changeList" type="atString"/>
	<size_t name="exeSize" />
	
	<array name="results" type="atArray">
		<pointer type="metricsList" policy="owner"/>
	</array>
</structdef>

</ParserSchema>