#! /usr/bin/perl -w

open(INFILE, "<:raw:encoding(UTF-16LE):crlf", $ARGV[0]) or die "Can't open file";

my $stringId = "";
my $english = "";
my $stringVal = "";
my $invalidString = "NOTEXTNOTEXTNOTEXT";

while (<INFILE>)
{
    if(/LocalizedString.*friendlyName\=\"(.*)\"/)
    {
        $stringId = $1;
        $english = "";
        $stringVal = "";
    }
    elsif(/<Translation\s+locale="en-US">(.*)<\/Translation>/s)
    {
        $english = $1;
    }

    if(/<Translation\s+locale="$ARGV[1]">(.*)<\/Translation>/s)
    {
        $stringVal = $1;
    }

    if(!($stringId =~ /X_STRINGID_.*/)
        || ($stringId =~ /X_STRINGID_TITLENAME/))
    {
        if($stringId && $english && $stringVal)
        {
			if(!($stringVal =~ $invalidString))
			{
				print "[" . $stringId . "]\n";
				if("en-US" ne $ARGV[1])
				{
					print "{" . $english . "}\n";
				}
				
				#print $stringVal . "\n\n";

				my $ustring2 = Encode::decode_utf8( $stringVal );
				binmode STDOUT, ":utf8";
				print $ustring2 . "\n\n";
			}
            $stringId = "";
        }
    }
}

