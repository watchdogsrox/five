#! /usr/bin/perl -w

my %strings = ();

open(INFILE, "<:raw:encoding(UTF-8):crlf", $ARGV[0]) or die "Can't open file";
open(XLAST_IN, "<:raw:encoding(UTF-16LE):crlf", $ARGV[1]) or die "Can't open file";
open(XLAST_OUT, ">:raw:encoding(UTF-16LE):crlf", $ARGV[2]) or die "Can't open file";

my $stringId = "";

if($ARGV[0] =~ /americanXLAST.txt/)
{
	$locale = "en-US";
}
elsif ($ARGV[0] =~ /chineseXLAST.txt/)
{
	$locale = "zh-CHT";
}
elsif ($ARGV[0] =~ /frenchXLAST.txt/)
{
	$locale = "fr-FR";
}
elsif ($ARGV[0] =~ /portugueseXLAST.txt/)
{
	$locale = "pt-PT";
}
elsif ($ARGV[0] =~ /italianXLAST.txt/)
{
	$locale = "it-IT";
}
elsif ($ARGV[0] =~ /germanXLAST.txt/)
{
	$locale = "de-DE";
}
elsif ($ARGV[0] =~ /japaneseXLAST.txt/)
{
	$locale = "ja-JP";
}
elsif ($ARGV[0] =~ /spanishXLAST.txt/)
{
	$locale = "es-ES";
}
elsif ($ARGV[0] =~ /koreanXLAST.txt/)
{
	$locale = "ko-KR";
}
elsif ($ARGV[0] =~ /russianXLAST.txt/)
{
	$locale = "ru-RU";
}
elsif ($ARGV[0] =~ /polishXLAST.txt/)
{
	$locale = "pl-PL";
}
else
{
	if($ARGV[0] =~ /americanXLAST.txt/)
	{
		$locale = "en-US";
	}
	else
	{
		$locale = "NONE";
	}
}

print $locale;

my $i = 0;

while (<INFILE>)
{
    chomp;
    if(/\[(.*)\]/)
    {
        $stringId = $1;
        $i = 1;
    }
    elsif(/\{.*\}$/ && 1 == $i)
    {
        $i = 2;
    }
    elsif(length)
    {
        $strings{$stringId} = $_;
        $stringId = "";
        $i = 0;
    }
}

my $inString = 0;

while(<XLAST_IN>)
{
    #chomp;
    #chomp;
    my $line = $_;

    if(1)
    {
		if(!$inString)
		{
			if(/LocalizedString.*friendlyName\=\"(.*)\"/)
			{
				$stringId = $1;
				$inString = 1;
			}
		}
		else
		{
			if(/(\s*\<Translation locale=\"$locale\"\>)[^\<]*(.*)/)
			{
				my $prefix = $1;
				my $suffix = $2;

				if(!($stringId =~ /X_STRINGID_.*/)
					|| ($stringId =~ /X_STRINGID_TITLENAME/))
				{
					my $translation = $strings{$stringId};
					if($translation)
					{
						$line = $prefix . $translation . $suffix . "\n";
					}
				}
			}
			elsif(/\<\/LocalizedString\>/)
			{
				$inString = 0;
			}
		}
    }

	#print $line;
    print XLAST_OUT $line;
}
