#!perl

use File::Find;
use File::Basename;

$dir_to_run_on = shift;
$max_size = shift;

if ( (! length( $dir_to_run_on ) || (! $max_size ) ) )
  {
	die "format is 'limit_vtf_sizes game_dir_to_run_on size'. maxisze -1 to unlimit";
  }

open(CMDOUT,">\runit.cmd");

find(\&ProcessFile, $dir_to_run_on);

close CMDOUT;

sub ProcessFile
  {
	local($_) = $File::Find::name;
	my $origname=$_;
	my $srcname;
	s@\\@/@g;
	if (/\.vtf$/i)				# is a vtf?
	  {
		next if (m@/hud/@i);	# don't shrink hud textures
		next if (m@/vgui/@i);	# don't shrink hud textures
		my $vtex_it=0;
		if ( s@game/(.*)/materials/@content/\1/materialsrc/@i )
		  {
			$srcname=$_;
			s/\.vtf$/\.txt/i;
			if (-e $_ )
			  {
				my $txtname=$_;
				# decide whether or not to add the limits
				open(TXTIN,$txtname) || die "can't open $_ for read something weird has happened";
				my $txtout;
				my $should_add_it=1;
				while( <TXTIN>)
				  {
					next if ( ( $max_size == -1) && (/maxwidth/i || /maxheight/i) ); # lose this line
					$txtout.=$_;
					$should_add_it = 0 if (/maxwidth/i || /maxheight/i || /nomip/i || /reduce/i );
				  }
				close TXTIN;
				if ($should_add_it )
				  {
					print `p4 edit $txtname`;
					open(TXTOUT,">$txtname");
					print TXTOUT $txtout;
					print TXTOUT "maxwidth $max_size\nmaxheight $max_size\n" if ($max_size != -1);
					close TXTOUT;
					$vtex_it = 1;
				  }
			  }
			else
			  {
				if (-d dirname($_) )
				  {
					print "$_ not found. Creating it\n";
					open(TXTOUT,">$_" ) || die "can't create $_?";
					print TXTOUT "maxwidth $max_size\nmaxheight $max_size\n" if( $max_size != -1);
					close TXTOUT;
					print `p4 add $_`;
					$vtex_it=1;
				  }
				else
				  {
					print "directory does not exist in content for $_\n";
				  }
			  }
		  }
		else
		  {
			die "I dont understand the file $_ : bad path?";
		  }
		if ($vtex_it)
		  {
			my $name=$srcname;
			$name=~s@\..*$@@;	# kill extension
			$cmd="vtex -nopause $name";
			$cmd=~s@/@\\@g;
			print "execute:$cmd\n";
			print CMDOUT "$cmd\n";
#			print `$cmd`."\n";
			
		  }

	  }
  }

