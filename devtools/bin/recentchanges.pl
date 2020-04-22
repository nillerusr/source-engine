# usage: recentchanges.pl <changelist number> <filespec>
# 
# Outputs changelist notes for changelists starting at the changelist number until now
# that involve files in the filespec, grouped by user.
# 
# Output file is recentchanges.txt, output to the current working dir.

BEGIN {use File::Basename; push @INC, dirname($0); }
require "valve_perl_helpers.pl";

# take in the revision number we want and the filespec to look at 
my $from_changelist = shift;
my $branch = shift;

if ( length($from_changelist) < 1 || length($branch) < 1  )
{
	die "usage: recentchanges.pl <'from' changelist number> <filespec>";
}

my $cmd = "p4 changes -l $branch\@$from_changelist,\@now";
my @fstat = &RunCommand( $cmd );

my $lastuser;
my %changes;
	
foreach $line ( @fstat )
{	
	#if the line starts with "Change"
	#then store the user, all lines after this are changes by this user
	if ( $line =~ m/^Change/ )
	{					
		my $data = 'Change (1234) on (date) by (name)@(clientspec)';

		my @values = split(' ', $line);
		
		@usersplit = split('@', $values[5]);	
		
		$lastuser = $usersplit[0];				
	}
	else
	{
		#this is a change line, associate it with the user in our %changes hash
		
		chomp($line);
		
		# skip empty and 'merge from main' lines
		if ( length($line) > 0 && $line !~ m/Merge from main/ )
		{		
			#strip leading whitespace
			$line =~ s/^\s+//;
			
			# if the first char is not '-', add '- ' to the front
			if ( $line !~ m/^-/ )
			{
				$line = "- $line";
			}
			
			push @{ $changes{$lastuser} }, $line;
		}
	}
}

chdir();
open(OUTFILE, ">recentchanges.txt");

foreach $user ( keys %changes )
{
	print "$user\n";
    print OUTFILE "$user\n";
    foreach $i ( 0 .. $#{ $changes{$user} } )
    {
		print "$changes{$user}[$i]\n";
		print OUTFILE "$changes{$user}[$i]\n";
    }
    print "\n";
    print OUTFILE "\n";
}
 
close(OUTFILE);