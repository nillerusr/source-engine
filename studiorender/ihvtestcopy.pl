$infile = shift;
$outfile = shift;

open INFILE, "<$infile";
@infile = <INFILE>;
close INFILE;

open OUTFILE, ">$outfile";
while( shift @infile )
{
	print OUTFILE $_;	
}
close OUTFILE;