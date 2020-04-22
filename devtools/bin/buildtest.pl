use strict;

sub DoBuild
{
	my @output;
	my $buildtype = shift;
	if( $buildtype eq "release" )
	{
		open BUILD, "dev_build_all.bat|";
	}
	elsif( $buildtype eq "debug" )
	{
		open BUILD, "dev_build_all.bat debug|";
	}
	else
	{
		die;
	}
	my $buildfailed = 0;
	while( <BUILD> )
	{
		if( /Build Errors\!/ )
		{
			$buildfailed = 1;
		}
		print;
		push @output, $_;
	}
	close build;

	if( $buildfailed )
	{
		open CHANGES, "p4 changes -m 10 -s submitted //ValveGames/main/src/...|";
		my @changes = <CHANGES>;
		close CHANGES;
		open EMAIL, ">email.txt";
		print EMAIL "LAST 10 SUBMITS TO MAIN:\n";
		print EMAIL @changes;
		print EMAIL "\n";
		my $line;
		foreach $line ( @output )
		{
			if( $line =~ m/error/i )
			{
				print EMAIL $line;
			}
		}
		print EMAIL "--------------------------------------------\n\n\n\n";
		print EMAIL @output;
		close EMAIL;
		system "devtools\\bin\\smtpmail.exe -to srcdev\@valvesoftware.com -from srcdev\@valvesoftware.com -subject \"FIX THE BUILD\! ($buildtype)\" -verbose email.txt";
#		system "devtools\\bin\\smtpmail.exe -to gary\@valvesoftware.com -from srcdev\@valvesoftware.com -subject \"FIX THE BUILD\! ($buildtype)\" -verbose email.txt";
	}
}

while( 1 )
{
	$ENV{"USE_INCREDIBUILD"} = "1";
	system "p4 sync > sync.txt 2>&1";
	my $hasChange = 1;
	my $line;
	open SYNC, "<sync.txt";
	while( <SYNC> )
	{
		if( m/File\(s\) up-to-date/ )		
		{
			$hasChange = 0;
		}
		print;
	}
	close SYNC;
	if( $hasChange )
	{
		print "changes checked in\n";
		&DoBuild( "release" );
		&DoBuild( "debug" );
	}
	else
	{
		print "no changes checked in\n";
		sleep 30;
	}
}