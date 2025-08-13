import xml.dom.minidom
import sys

filename = "X:/gta5/xlast/Leaderboards.ros.xml"

if (len(sys.argv) > 1):
	filename = sys.argv[1]

anXMLDocument = xml.dom.minidom.parse(filename)

#############################################################
#   PROPERTIES => INPUTS
PropertiesDict = dict()
PropertyMasterNode = anXMLDocument.getElementsByTagName("Properties")
PropertyNodeList = PropertyMasterNode[0].getElementsByTagName("Property")
for prop in PropertyNodeList:
	if prop.nodeType == prop.ELEMENT_NODE:
		#propetryId = prop.attributes["id"].value
		friendlyName = prop.attributes["friendlyName"].value
		dataType = "int"
		if prop.attributes["dataSize"].value == "8":
			if prop.getElementsByTagName("Format")[0].attributes.has_key("decimals"):
				dataType = "double"
			else:
				dataType = "long"
		
		PropertiesDict[prop.attributes["id"].value] = (prop.attributes["id"].value, friendlyName, dataType)
		
###################################################################
#
#	Contexts -> Categories
#
ContextListsXMLNodes = anXMLDocument.getElementsByTagName("Context")
#get the one named 'LeaderboardCategory'
for cxtNode in ContextListsXMLNodes:
	if cxtNode.nodeType == cxtNode.ELEMENT_NODE and cxtNode.attributes["friendlyName"].value == "LeaderboardCategory":
		ContextValueXMLNodes = cxtNode.getElementsByTagName("ContextValue")
		CategoryList = list()
		for cxt in ContextValueXMLNodes:
			if cxt.nodeType == cxt.ELEMENT_NODE:
				CategoryList.append(cxt.attributes["friendlyName"].value)

###################################################################
#     LEADERBOARDS
#
#Get a list of all the elements of the given type
ViewMasterNode = anXMLDocument.getElementsByTagName("StatsViews")

LBList = list()

class ComulmnData:
	friendlyName = 'Invalid Name'
	aggType = 'Agg Type'
	PropId = 'Old id'
	oldAttributeId = '65454'
	conditionalOnAttrId = ""
	newOrdinal = 0
	newColId = 0
	conditionalOnColId = 0
	ratioAttribId1 = ""
	ratioAttribId2 = ""
	ratioColId1 = 0
	ratioColId2 = 0

for vm in ViewMasterNode:
	
	#Get the list of StatsViews
	for sv in vm.childNodes:
		if sv.nodeType == sv.ELEMENT_NODE:
			
			#Create the columns list.
			xmlColumnList = sv.getElementsByTagName("Field")
			columnList = list()
			Ordinal = 0
			for col in xmlColumnList:
				if col.nodeType == sv.ELEMENT_NODE:
					friendlyName = col.attributes["friendlyName"].value
					if friendlyName == "RANK_COLUMN" or friendlyName == "GAMERNAME_COLUMN":
						continue
						
					property = col.getElementsByTagName("Property")
					agg = col.getElementsByTagName("Aggregation")
						
					newColData = ComulmnData()
					newColData.friendlyName = friendlyName
					newColData.aggType = agg[0].attributes["type"].value
					newColData.PropId = property[0].attributes["id"].value
					newColData.oldAttributeId = col.attributes["attributeId"].value
					newColData.newOrdinal = Ordinal
					newColData.newColId = Ordinal + 1
					if agg[0].attributes.has_key("conditionalOn"):
						newColData.conditionalOnAttrId = agg[0].attributes["conditionalOn"].value
					else:
						newColData.conditionalOnAttrId = ""
					
					if newColData.aggType == "Ratio":
						AggInputs = agg[0].getElementsByTagName("Input")
						newColData.ratioAttribId1 = AggInputs[0].attributes["attributeId"].value
						newColData.ratioAttribId2 = AggInputs[1].attributes["attributeId"].value
					
					columnList.append(newColData)
					
					Ordinal += 1
			
			#now that we have the columns, do a fixup for the conditionalOn columns
			for colData in columnList:
				if colData.conditionalOnAttrId != "":
					#find the column in this list with this oldAttributeId
					for condOnIter in columnList:
						if condOnIter.oldAttributeId == colData.conditionalOnAttrId:
							colData.conditionalOnColId = condOnIter.newColId
							break
			#fixup for ration columns
			for colData in columnList:
				if colData.aggType == "Ratio":
					for condOnIter in columnList:
						if condOnIter.oldAttributeId == colData.ratioAttribId1:
							colData.ratioColId1 = condOnIter.newColId
							continue
						if condOnIter.oldAttributeId == colData.ratioAttribId2:
							colData.ratioColId2 = condOnIter.newColId
							continue
			
			#create the instances list
			instanceList = list()
			for inst in sv.getElementsByTagName("Instance"):
				if inst.nodeType == inst.ELEMENT_NODE:
					instanceList.append(inst.firstChild.data)
					
			#add this to the master list of leaderboards		
			LBList.append((sv.attributes["id"].value, sv.attributes["friendlyName"].value, columnList, instanceList))
			
	
#now figure out which of the properties we need based on which ones are referenced in the LBs
UsedPropertiesSet = set()
for (_id, lbName, colList, _instList) in LBList:
	for colData in colList:
		UsedPropertiesSet.add(colData.PropId)
		
#use the map() stuff to get the list of tuples that is sorted alphabetically by friendlyName (the second item in the tuple)
def mapFunc(x) : return PropertiesDict[x]
SortedUsedPropertyList = sorted( map(mapFunc, UsedPropertiesSet), key=lambda tup: tup[1])


#############################################################################
#
#   Now let's dump out some Xml, shall we?
#	
doc = xml.dom.minidom.Document()
LBConfigNode = doc.createElement("LeaderboardsConfiguration")
doc.appendChild(doc.createElement("RockstarScsGameConfiguration")).appendChild(LBConfigNode)

#
# XML for Inputs
#
INPUTXmlNode = doc.createElement("Inputs")
LBConfigNode.appendChild(INPUTXmlNode)

PropertyIdToInputId = dict()
InputCount = 1
for (_hex, name, dtype) in SortedUsedPropertyList:
	InputNode = doc.createElement("Input")
	InputNode.setAttribute("id", str(InputCount))
	InputNode.setAttribute("name", name)
	InputNode.setAttribute("dataType", dtype)
	INPUTXmlNode.appendChild(InputNode)
	
	PropertyIdToInputId[_hex] = InputCount
	
	InputCount += 1
	
#
#	XML for Categories
#
CategoriesXMLNode = doc.createElement("Categories")
LBConfigNode.appendChild(CategoriesXMLNode)
for ctgy in CategoryList:
	categoryNode = doc.createElement("Category")
	categoryNode.setAttribute("name", ctgy)
	CategoriesXMLNode.appendChild(categoryNode)
	

#
#	XML for Leaderboards
#
LBSXmlNode = doc.createElement("Leaderboards")
LBConfigNode.appendChild(LBSXmlNode)

for (id, lbName, colList, instList) in LBList:
	LBNode = doc.createElement("Leaderboard")
	LBNode.setAttribute("id", id)
	LBNode.setAttribute("name", lbName)
	LBNode.setAttribute("entryExpiration", "0")
	LBNode.setAttribute("resetType", "Never")
	
	CategoryPermsNode = doc.createElement("CategoryPermutations ")
	LBNode.appendChild(CategoryPermsNode)
	for inst in instList:
		instanceNode = doc.createElement("CategoryPermutation")
		instanceNode.appendChild(doc.createTextNode(inst))
		CategoryPermsNode.appendChild(instanceNode)
		
	ColumnsNode = doc.createElement("Columns")
	LBNode.appendChild(ColumnsNode)
	for colData in colList: #(colName, aggType, propId) 
		if colData.friendlyName == "RANK_COLUMN" or colData.friendlyName == "GAMERNAME_COLUMN":
			continue
			
		ColNode = doc.createElement("Column")
		ColumnsNode.appendChild(ColNode)
		ColNode.setAttribute("name", colData.friendlyName)
		ColNode.setAttribute("id", str(colData.newColId))
		ColNode.setAttribute("ordinal", str(colData.newOrdinal))
		if colData.friendlyName == "SCORE_COLUMN":
			ColNode.setAttribute("isRankingColumn", "True")
		AggNode = doc.createElement("Aggregation")
		ColNode.appendChild(AggNode)
		AggNode.setAttribute("type", colData.aggType)
		if colData.conditionalOnColId != 0:
			AggNode.setAttribute("conditionalOnColumnId", str(colData.conditionalOnColId))
		if colData.aggType == "Ratio":
			ColInputNode = doc.createElement("ColumnInput")
			ColInputNode.setAttribute("columnId", str(colData.ratioColId1))
			ColInputNode.setAttribute("ordinal", str(1))
			AggNode.appendChild(ColInputNode)
			ColInputNode = doc.createElement("ColumnInput")
			ColInputNode.setAttribute("columnId", str(colData.ratioColId2))
			ColInputNode.setAttribute("ordinal", str(2))
			AggNode.appendChild(ColInputNode)
		else:
			GameInputNode = doc.createElement("GameInput")
			AggNode.appendChild(GameInputNode)
			GameInputNode.setAttribute("inputId", str(PropertyIdToInputId[colData.PropId]))

	LBSXmlNode.appendChild(LBNode)


	#for sv in statViewsList
	#	valueOfNode = e.childNodes[0].nodeValue
	#print valueOfNode

print doc.toprettyxml()

