

$root = "x:\Dev\core\shader_source\"
#$root = "..\..\..\core\shader_source\"

function Dump-ProjectDeptails( $key, $dep )
{
	gc template.txt | %{ $_.Replace( "%name%",$key).Replace("%dep%",$dep) }
}
function RecurseKey( $key , $showKey = $false)
{
	if ( $script:doneTable.Contains($key) )
	{
		return;
	}
	
	if ( $showKey )
	{
		$res = $root + $key + ";"
	}
	
	$script:doneTable[$key]= $key

	if ( $link.Contains( $key ) )
	{				
		$link[$key] | %{ $Res += RecurseKey $_ $true }
	}
	
	return $res
}
function Strip-Include( $l )
{
	$c = $l.TrimStart().Substring("#include".Length).TrimStart("`" ")

	return $c.Substring(0,$c.IndexOf("`"")).TrimStart().TrimEnd()
}
function Get-RecursiveDependances( $key )
{
	
	$script:doneTable.Clear()

	#$key + "	->"

	$result = RecurseKey $key 

	return $result
}

$script:doneTable = @{"empty.fxh"=$true}
$link = @{ }

$key="empty.fxh"
$line=""

# first build up first level table
select-string -path *.fx,*.fxh -pattern "^[ \t]*#include*" | `
	 %{ $key=$_.Filename; $v =Strip-Include( $_.Line ) ; `
		[string[]]$link[$key] += [string[]]$v  }


# and show it
#$link

gc header.txt

$link.keys | %{ if ( $_.EndsWith(".fx") ) { $dep = Get-RecursiveDependances $_ ; Dump-ProjectDeptails $_ $dep } }

gc footer.txt



