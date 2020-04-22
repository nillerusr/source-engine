use Cwd;

my $dir = getcwd;
 
chdir "../../materialsystem/stdshaders";
 
@output = `perl ..\\..\\devtools\\bin\\checkshaderchecksums.pl stdshader_dx9_20b.txt`;
foreach $_ (@output)
{
  $output.=$_ unless(/appchooser360/i);
}

@output = `perl ..\\..\\devtools\\bin\\checkshaderchecksums.pl stdshader_dx9_30.txt`;
foreach $_ (@output)
{
  $output.=$_ unless(/appchooser360/i);
}

my $errors;

foreach $_ (@output )
{
  $errors.=$_ unless (/appchooser360movie/);
}

chdir $dir;

print $errors;

if( length( $errors ) > 0 )
{
	print "writing errors.txt\n";
	open FP, ">errors.txt";
	print FP "$errors";
	close FP;
}
