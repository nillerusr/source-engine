
# This script sets up VMPI.

$pathToExes = "..\\..\\..\\game\\bin"; # assuming we're under hl2\src\devtools\bin.
my $baseNetworkPath = @ARGV[0];

if ( $baseNetworkPath )
{
	my @filesToCopy = ( 
		"tier0.dll", 
		"vstdlib.dll", 
		"vmpi_service.exe", 
		"vmpi_service_ui.exe",
		"vmpi_service_install.exe", 
		"WaitAndRestart.exe",
		"vmpi_transfer.exe",
		"filesystem_stdio.dll" );

	# Verify that the files we're interested in exist.
	foreach $filename ( @filesToCopy )
	{
		my $fullFilename = "$pathToExes\\$filename";
		if ( !( -e $fullFilename ) )
		{
			die "Missing $fullFilename.\n";
		}
	}

	# Create the directories on the network.
	if ( !( -e $baseNetworkPath ) )
	{
		mkdir $baseNetworkPath or die "Can't create directory: $baseNetworkPath";
	}

	if ( !( -e "$baseNetworkPath\\services" ) )
	{
		mkdir( "$baseNetworkPath\\services" ) or die "ERROR: Can't create directory: $baseNetworkPath\\services";
	}


	# Now copy all the files up.
	foreach $filename ( @filesToCopy )
	{
		my $fullFilename = "$pathToExes\\$filename";
		`"copy $fullFilename $baseNetworkPath\\services"`;
	}

	print "\n";
	print "Finished installing VMPI into $baseNetworkPath.\n\n";
	print "Have all available worker machines run:\n";
	print "    $baseNetworkPath\\services\\vmpi_service_install.exe\n";
	print "to setup the worker service.\n";
}
else
{
	print "setup_vmpi.pl <UNC path to a server all machines can access>\n";
	print "example: setup_vmpi.pl \\\\ftknox\\scratch\\vmpi\n";
}


