use Cwd;

my $dir = getcwd;

chdir "../../../game";

if( 1 )
{
	system "rd /s /q ep2\\screenshots";
	system "mkdir ep2\\screenshots";
	@output = `hl2.exe -allowdebug -autoconfig -console -toconsole -dev -sw -width 1024 -game ep2 -testscript rendering_regression_test.vtest`;
}

$keydir = "\\\\fileserver\\user\\rendering_regression_test";

open TESTSCRIPT, "<ep2/testscripts/rendering_regression_test.vtest" || die;
foreach $line (<TESTSCRIPT>)
{
	$line =~ s,//.*,,g; # remove comments
	if( $line =~ m/\s*screenshot\s+(.*)$/i )
	{
		push @screenshots, $1;
	}
}
close TESTSCRIPT;

foreach $screenshot (@screenshots)
{
	$cmd = "tgamse $keydir\\$screenshot.tga ep2\\screenshots\\$screenshot.tga 0";
	$output = `$cmd`;
	if( $output =~ m/FAIL/ )
	{
		$cmd = "tgadiff $keydir\\$screenshot.tga ep2\\screenshots\\$screenshot.tga ep2\\screenshots\\$screenshot" . "_diff.tga";
		system $cmd;
	}
}
