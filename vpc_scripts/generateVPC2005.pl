use strict;
no strict 'refs';

######################################################
# Data Section
######################################################

my(@lines)		= ();	# holds the input file	
my(@tabs) 		= ();	# for indentation
my(@lineToPrint)	= ();	# current formatted output line
#my(@configOutput)	= ();	# output from configuration parsing
my(%configOutput)	= ();	# hash of configuration-specific config options
my($currentConfig)	= "";	# configuration currently being parsed
my($nameFound) 		= 0;	# flag set when project name has been parsed
my($lineCt) 		= 0;	# current line number in the source file
my($exclusionsFound)	= 0;	# an file has been 'excluded from build' for some configuration
my($projectName)	= "";	# base name of the project
my($parsingFiles)	= 0;	# a cheap state variable
my($splitConfigs)	= 0;	# debug variable set from the commmand line
my($outputPath)		= undef;# optional path for the vpc destination
my($spaceBeforeFolder)	= 0;	# tracks when to add a line before folder blocks
my($spaceBeforeFile)	= 0;	# tracks when to add a line before file blocks
my($tabstop)		= 4;	# size of tabs - 4 for visual studio
my($srcdirBackslash)	= undef;# holds the value of the macro SRCDIR, with \ separators
my($srcdirForwardslash)	= undef;# holds the value of the macro SRCDIR, with / separators

my($usestring)		= 1;	# use string values for compiler options
my($stripEmptyOptions)	= 1;	# remove compiler options that have empty values (strings)
if ( $ARGV[1] =~ /useindex/ )
{
	$usestring = 0;
}
elsif ( $ARGV[1] =~ /allownullstrings/ )
{
	$stripEmptyOptions = 0;
}
elsif ( $ARGV[1] =~ /-o/ )
{
	$outputPath = $ARGV[2];
}


##############################################################################
# Match vcproj option keywords with vpc keywords - only string value options
# String on the left matches the option name in the VCProj.
# String on the right matches the option name in vpc

my(%configOptionsSingleValue) = (

	# Configuration
	"General"			=>
	{
		# General	
		"OutputDirectory"			=>	"OutputDirectory",
		"IntermediateDirectory"			=>	"IntermediateDirectory",
		"DeleteExtensionsOnClean"		=>	"ExtensionsToDeleteOnClean",
		"BuildLogFile"				=>	"BuildLogFile",
		"ATLMinimizesCRunTimeLibraryUsage"	=>	"MinimizeCRTUseInATL",
		"ManagedExtensions"			=>	"UseManagedExtensions",
		"WholeProgramOptimization"		=>	"WholeProgramOptimization",
		"ReferencesPath"			=>	"ReferencesPath",
		"ExcludedFromBuild"			=>	"ExcludedFromBuild",

		# Debugging
	},

	# C/C++ 
	"VCCLCompilerTool"		=>
	{
		# General
		"AdditionalIncludeDirectories"		=>	"AdditionalIncludeDirectories",
		"AdditionalUsingDirectories"		=>	"ResolveUsingReferences",

		# Optimization
		
		# Preprocessor
		"PreprocessorDefinitions"		=>	"PreprocessorDefinitions",

		# Code Generation

		# Language

		# Precompiled Headers
		"PrecompiledHeaderThrough"		=>	"Create/UsePCHThroughFile",
		"PrecompiledHeaderFile"			=>	"PrecompiledHeaderFile",

		# Output Files
		"AssemblerListingLocation"		=>	"ASMListLocation",
		"ObjectFile"				=>	"ObjectFileName",
		"ProgramDataBaseFileName"		=>	"ProgramDatabaseFileName",
		"XMLDocumentationFileName"		=>	"XMLDocumentationFileName",

		# Browse Information
		"BrowseInformationFile"			=>	"BrowseFile",

		# Advanced
		"DisableSpecificWarnings"		=>	"DisableSpecificWarnings",
		"ForcedIncludeFiles"			=>	"ForceIncludes",
		"ForcedUsingFiles"			=>	"ForceUsing",
		"UndefinePreprocessorDefinitions"	=>	"UndefinePreprocessorDefinitions",

		# Command Line
		"AdditionalOptions"			=>	"AdditionalOptions",
	},

	# Librarian
	"VCLibrarianTool"	=>
	{
		# General
		"OutputFile"				=>	"OutputFileexplo",
		"AdditionalDependencies"		=>	"AdditionalDependencies",
		"AdditionalLibraryDirectories"		=>	"AdditionalLibraryDirectories",
		"ModuleDefinitionFile"			=>	"ModuleDefinitionFileName",
		"IgnoreDefaultLibraryNames"		=>	"IgnoreSpecificLibrary",
		"ExportNamedFunctions"			=>	"ExportNamedFunctions",
		"ForceSymbolReferences"			=>	"ForceSymbolReferences",

		# Command LIne
		"AdditionalOptions"			=>	"AdditionalOptions",
	},

	# Linker 
	"VCLinkerTool"			=>
	{
		# General
		"OutputFile"				=>	"OutputFile",
		"Version"				=>	"Version",
		"AdditionalLibraryDirectories"		=>	"AdditionalLibraryDirectories",

		# Input
		"AdditionalDependencies"		=>	"AdditionalDependencies",
		"IgnoreDefaultLibraryNames"		=>	"IgnoreSpecificLibrary",
		"ModuleDefinitionFile"			=>	"ModuleDefinitionFile",
		"AddModuleNamesToAssembly"		=>	"AddModuleToAssembly",
		"EmbedManagedResourceFile"		=>	"EmbedManagedResourceFile",
		"ForceSymbolReferences"			=>	"ForceSymbolReferences",
		"DelayLoadDLLs"				=>	"DelayLoadedDLLs",
		"AssemblyLinkResource"			=>	"AssemblyLinkResource",

		# Manifest File
		"ManifestFile"				=>	"ManifestFile",
		"AdditionalManifestDependencies"	=>	"AdditionalManifestDependencies",

		# Debugging
		"ProgramDatabaseFile"			=>	"GenerateProgramDatabaseFile",
		"StripPrivateSymbols"			=>	"StripPrivateSymbols",
		"MapFileName"				=>	"MapFileName",
	
		# System
		"HeapReserveSize"			=>	"HeapReserverSize",
		"HeapCommitSize"			=>	"HeapCommitSize",
		"StackReserveSize"			=>	"StackReserveSize",
		"StackCommitSize"			=>	"StackCommitSize",

		# Optimization
		"FunctionOrder"				=>	"FunctionOrder",
		"ProfileGuidedDatabase"			=>	"ProfileGuidedDatabase",

		# Embedded IDL
		"MidlCommandFile"			=>	"MIDLCommands",
		"MergedIDLBaseFileName"			=>	"MergedIDLBaseFileName",
		"TypeLibraryFile"			=>	"TypeLibrary",
		"TypeLibraryResourceID"			=>	"TypeLibResourceID",

		# Advanced
		"EntryPointSymbol"			=>	"EntryPoint",
		"BaseAddress"				=>	"BaseAddress",
		"ImportLibrary"				=>	"ImportLibrary",
		"MergeSections"				=>	"MergeSections",
		"KeyFile"				=>	"KeyFile",
		"KeyContainer"				=>	"KeyContainer",

		# Command LIne
		"AdditionalOptions"			=>	"AdditionalOptions",
	},

	# Resources
	"VCResourceCompilerTool"	=>
	{
		# General
		"PreprocessorDefinitions"		=>	"PreprocessorDefinitions",
		"AdditionalIncludeDirectories"		=>	"AdditionalIncludeDirectories",
		"ResourceOutputFileName"		=>	"ResourceFileName",

		# Command LIne
		"AdditionalOptions"			=>	"AdditionalOptions",
	},

	# Build Events
	"VCPreBuildEventTool"		=>
	{
		# Pre-Build Event
		"CommandLine"				=>	"CommandLine",
		"Description"				=>	"Description",
		"ExcludedFromBuild"			=>	"ExcludedFromBuild",
	},

	"VCPreLinkEventTool"		=>
	{
		# Pre-Link Event
		"CommandLine"				=>	"CommandLine",
		"Description"				=>	"Description",
		"ExcludedFromBuild"			=>	"ExcludedFromBuild",
	},

	"VCPostBuildEventTool"		=>
	{
		# Post-Build Event
		"CommandLine"				=>	"CommandLine",
		"Description"				=>	"Description",
		"ExcludedFromBuild"			=>	"ExcludedFromBuild",
	},

	# Custom Build Step
	"VCCustomBuildTool"		=>
	{
		# Pre-Build Event
		"CommandLine"				=>	"CommandLine",
		"Description"				=>	"Description",
		"Outputs"				=>	"Outputs",
		"AdditionalDependencies"		=>	"AdditionalDependencies",
	},

	);

##############################################################################
# Match vcproj option keywords with vpc keywords - only multi-value options
# String on the left matches the option name in the VCProj.
# String on the right matches the option name in vpc

my(%configOptionsMultiValue) = (

		# General	
		"ConfigurationType"			=>	"ConfigurationType",
		"UseOfMFC"				=>	"UseOfMFC",
		"UseOfAtl"				=>	"UseOfATL",
		"CharacterSet"				=>	"CharacterSet",

		# Debugging

	# C/C++ 
		# General
		"DebugInformationFormat"		=>	"DebugInformationFormat",
		"SuppressStartupBanner"			=>	"SuppressStartupBanner",
		"WarningLevel"				=>	"WarningLevel",
		"Detect64BitPortabilityProblems"	=>	"Detect64BitPortabilityIssues",
		"WarnAsError"				=>	"TreatWarningsAsErrors",
		"UseUnicodeResponseFiles"		=>	"UseUNICODEResponseFiles",

		# Optimization
		"Optimization"				=>	"Optimization",
		"InlineFunctionExpansion"		=>	"InlineFunctionExpansion",
		"EnableIntrinsicFunctions"		=>	"EnableIntrinsicFunctions",
		"FavorSizeOrSpeed"			=>	"FavorSizeOrSpeed",
		"OmitFramePointers"			=>	"OmitFramePointers",
		"EnableFiberSafeOptimizations"		=>	"EnableFiberSafeOptimizations",
		"WholeProgramOptimization"		=>	"WholeProgramOptimization",
		
		# Preprocessor
		"IgnoreStandardIncludePath"		=>	"IgnoreStandardIncludePath",
		"GeneratePreprocessedFile"		=>	"GeneratePreprocessedFile",
		"KeepComments"				=>	"KeepComments",

		# Code Generation
		"StringPooling"				=>	"EnableStringPooling",
		"MinimalRebuild"			=>	"EnableMinimalRebuild",
		"ExceptionHandling"			=>	"EnableC++Exceptions",
		"SmallerTypeCheck"			=>	"SmallerTypeCheck",
		"BasicRuntimeChecks"			=>	"BasicRuntimeChecks",
		"RuntimeLibrary"			=>	"RuntimeLibrary",
		"StructMemberAlignment"			=>	"StructMemberAlignement",
		"BufferSecurityCheck"			=>	"BufferSecurityCheck",
		"EnableFunctionLevelLinking"		=>	"EnableFunctionLevelLinking",
		"EnableEnhancedInstructionSet"		=>	"EnableEnhancedInstructionSet",
		"FloatingPointModel"			=>	"FloatingPointModel",
		"FloatingPointExceptions"		=>	"EnableFloatingPointExceptions",

		# Language
		"DisableLanguageExtensions"		=>	"DisableLanguageExtensions",
		"DefaultCharIsUnsigned"			=>	"DefaultCharUnsigned",
		"TreatWChar_tAsBuiltInType"		=>	"TreatWchar_tAsBuiltinType",
		"ForceConformanceInForLoopScope"	=>	"ForceConformanceInForLoopScope",
		"RuntimeTypeInfo"			=>	"EnableRunTimeTypeInfo",
		"OpenMP"				=>	"OpenMPSupport",

		# Precompiled Headers
		"UsePrecompiledHeader"			=>	"Create/UsePrecompiledHeader",

		# Output Files
		"ExpandAttributedSource"		=>	"ExpandAttributedSource",
		"AssemblerOutput"			=>	"AssemblerOutput",
		"GenerateXMLDocumentationFiles"		=>	"GenerateXMLDocumentationFiles",

		# Browse Information
		"BrowseInformation"			=>	"EnableBrowseInformation",

		# Advanced
		"CallingConvention"			=>	"CallingConvention",
		"CompileAs"				=>	"CompileAs",
		"UndefineAllPreprocessorDefinitions"	=>	"UndefineAllPreprocessorDefinitions",
		"UseFullPaths"				=>	"UseFullPaths",
		"OmitDefaultLibName"			=>	"OmitDefaultLibraryNames",
		"ErrorReporting"			=>	"ErrorReporting",

	# Librarian
		# General
		"SuppressStartupBanner"			=>	"SuppressStartupBanner",
		"IgnoreAllDefaultLibraries"		=>	"IgnoreAllDefaultLibraries",
		"UseUnicodeResponseFiles"		=>	"UseUNICODEResponseFiles",
		"LinkLibraryDependencies"		=>	"LinkLibraryDependencies",
		
	# Linker
		# General
		"ShowProgress"				=>	"ShowProgress",
		"LinkIncremental"			=>	"EnableIncrementalLinking",
		"SuppressStartupBanner"			=>	"SuppressStartupBanner",
		"IgnoreImportLibrary"			=>	"IgnoreImportLibrary",
		"RegisterOutput"			=>	"RegisterOutput",
		"LinkLibraryDependencies"		=>	"LinkLibraryDependencies",
		"UseLibraryDependencyInputs"		=>	"UseLibraryDependencyInputs",
		"UseUnicodeResponseFiles"		=>	"UseUNICODEResponseFiles",

		# Input
		"IgnoreAllDefaultLibraries"		=>	"IgnoreAllDefaultLibraries",

		# Manifest File
		"GenerateManifest"			=>	"GenerateManifest",
		"AllowIsolation"			=>	"AllowIsolation",

		# Debugging
		"GenerateDebugInformation"		=>	"GenerateDebugInfo",
		"GenerateMapFile"			=>	"GenerateMapFile",
		"MapExports"				=>	"MapExports",
		"AssemblyDebug"				=>	"DebuggableAssembly",

		# System
		"SubSystem"				=>	"SubSystem",
		"LargeAddressAware"			=>	"EnableLargeAddresses",
		"TerminalServerAware"			=>	"TerminalServer",
		"SwapRunFromCD"				=>	"SwapRunFromCD",
		"SwapRunFromNet"			=>	"SwapRunFromNetwork",
		"Driver"				=>	"Driver",

		# Optimization
		"OptimizeReferences"			=>	"References",
		"EnableCOMDATFolding"			=>	"EnableCOMDATFolding",
		"OptimizeForWindows98"			=>	"OptimizeForWindows98",
		"LinkTimeCodeGeneration"		=>	"LinkTimeCodeGeneration",

		# Embedded IDL
		"IgnoreEmbeddedIDL"			=>	"IgnoreEmbeddedIDL",

		# Advanced
		"ResourceOnlyDLL"			=>	"NoEntryPoint",
		"SetChecksum"				=>	"SetChecksum",
		"FixedBaseAddress"			=>	"FixedBaseAddress",
		"TurnOffAssemblyGeneration"		=>	"TurnOffAssemblyGeneration",
		"SupportUnloadOfDelayLoadedDLL"		=>	"DelayLoadedDLL",
		"TargetMachine"				=>	"TargetMachine",
		"Profile"				=>	"Profile",
		"CLRThreadAttribute"			=>	"CLRThreadAttribute",
		"CLRImageType"				=>	"CLRImageType",
		"DelaySign"				=>	"DelaySign",
		"ErrorReporting"			=>	"ErrorReporting",
		"CLRUnmanagedCodeCheck"			=>	"CLRUnmanagedCodeCheck",

	# Resources
		# General
		"Culture"				=>	"Culture",
		"IgnoreStandardIncludePath"		=>	"IgnoreStandardIncludePath",
		"ShowProgress"				=>	"ShowProgress",
	);

##############################################################################
# Match user option names to their lists of possible values

my(%configOptionValues) = (

		# General
		"ConfigurationType"			=> 
		{		
			"0"	=>	"Makefile",
			"1"	=>	"Application \(\.exe\)",
			"2"	=>	"Dynamic Library \(\.dll\)",
			"3"	=>	"Static Library \(\.lib\)",
			"4"	=>	"Utility",
		},
		"UseOfMFC"				=> 
		{
			"0"	=>	"Use Standard Windows Libraries",
			"1"	=>	"Use MFC In A Static Library",
			"2"	=>	"Use MFC In A Shared DLL",
		},
		"UseOfATL"				=> 
		{
			"0"	=>	"Not Using ATL",
			"1"	=>	"Static Link To ATL",
			"2"	=>	"Dynamic Link To ATL",
		},
		"CharacterSet"				=> 
		{
			"0"	=>	"Not Set",
			"1"	=>	"Use Unicode Character Set",
			"2"	=>	"Use Multi-Byte Character Set",
		},

		# Debugging

	# C/C++ 
		# General
		"DebugInformationFormat"		=> 
		{
			# These skip a number on purpose (per VS2005) 
			"0"	=>	"Disabled",
			"1"	=>	"C7 Compatible \(\/Z7\)",
			"3"	=>	"Program Database \(\/Zi\)",
			"4"	=>	"Program Database for Edit & Continue \(\/ZI\)",
		},
		"SuppressStartupBanner"			=>
		{
			"false"	=>	"No",
			"true"	=>	"Yes \(\/nologo\)",
		},
		"WarningLevel"				=>
		{
			"0"	=>	"Off: Turn Off All Warnings \(\/W0\)",
			"1"	=>	"Level 1 \(\/W1\)",
			"2"	=>	"Level 2 \(\/W2\)",
			"3"	=>	"Level 3 \(\/W3\)",
			"4"	=>	"Level 4 \(\/W4\)",
		},
		"Detect64BitPortabilityProblems"	=>
		{
			"false"	=>	"No",
			"true"	=>	"Yes \(\/Wp64\)",
		},
		"WarnAsError"				=>
		{
			"false"	=>	"No",
			"true"	=>	"Yes \(\/WX\)",
		},
		"UseUnicodeResponseFiles"		=>
		{
			"false"	=>	"No",
			"true"	=>	"Yes",
		},

		# Optimization
		"Optimization"	=> 
		{
			"0"	=>	"Disabled \(\/Od\)",
			"1"	=>	"Minimize Size \(\/O1\)",
			"2"	=>	"Maximize Speed \(\/O2\)",
			"3"	=>	"Full Optimization \(\/Ox\)",
			"4"	=>	"Custom",
		},
		"InlineFunctionExpansion"		=>
		{
			"0"	=>	"Default",
			"1"	=>	"Only __inline \(\/Ob1\)",
			"2"	=>	"Any Suitable \(\/Ob2\)",
		},
		"EnableIntrinsicFunctions"		=>
		{
			"false"	=>	"No",
			"true"	=>	"Yes \(\/Oi\)",
		},
		"FavorSizeOrSpeed"			=>
		{
			"0"	=>	"Neither",
			"1"	=>	"Favor Fast Code \(\/Ot\)",
			"2"	=>	"Favor Small Code \(\/Os\)",
		},
		"OmitFramePointers"			=>
		{
			"false"	=>	"No",
			"true"	=>	"Yes \(\/Oy\)",
		},
		"EnableFiberSafeOptimizations"		=>
		{
			"false"	=>	"No",
			"true"	=>	"Yes \(\/GT\)",
		},
		"WholeProgramOptimization"		=>
		{
			"false"	=>	"No",
			"true"	=>	"Enable link-time code generation \(\/GL\)",
		},

		# Preprocessor
		"IgnoreStandardIncludePath"		=>
		{
			"false"	=>	"No",
			"true"	=>	"Yes \(\/X\)",
		},
		"GeneratePreprocessedFile"		=>
		{
			"0"	=>	"No",
			"1"	=>	"With Line Numbers \(\/P\)",
			"2"	=>	"Without Line Numbers \(\/EP \/P\)",
		},
		"KeepComments"				=>
		{
			"false"	=>	"No",
			"true"	=>	"Yes \(\/C\)",
		},

		# Code Generation
		"StringPooling"				=>
		{
			"false"	=>	"No",
			"true"	=>	"Yes \(\/GF\)",
		},
		"MinimalRebuild"			=>
		{
			"false"	=>	"No",
			"true"	=>	"Yes \(\/Gm\)",
		},
		"ExceptionHandling"			=>
		{
			"0"	=>	"No",
			"1"	=>	"Yes \(\/EHsc\)",
			"2"	=>	"Yes With SEH Exceptions \(\/EHa\)",
		},
		"SmallerTypeCheck"			=>
		{
			"false"	=>	"No",
			"true"	=>	"Yes \(\/RTCc\)",
		},
		"BasicRuntimeChecks"			=>
		{
			"0"	=>	"Default",
			"1"	=>	"Stack Frames \(\/RTCs\)",
			"2"	=>	"Uninitialized Variables \(\/RTCu\)",
			"3"	=>	"Both \(\/RTC1, equiv\. to \/RTCsu\)",
		},
		"RuntimeLibrary"			=>
		{
			"0"	=>	"Multi-threaded \(\/MT\)",
			"1"	=>	"Multi-threaded Debug \(\/MTd\)",
			"2"	=>	"Multi-threaded DLL \(\/MD\)",
			"3"	=>	"Multi-threaded Debug DLL \(\/MDd\)",
		},
		"StructMemberAlignment"			=>
		{
			"0"	=>	"Default",
			"1"	=>	"1 Byte \(\/Zp1\)",
			"2"	=>	"2 Bytes \(\/Zp2\)",
			"3"	=>	"4 Bytes \(\/Zp4\)",
			"4"	=>	"8 Bytes \(\/Zp8\)",
			"5"	=>	"16 Bytes \(\/Zp16\)",
		},
		"BufferSecurityCheck"			=>
		{
			"false"	=>	"No (/GS-)",
			"true"	=>	"Yes",
		},
		"EnableFunctionLevelLinking"		=>
		{
			"false"	=>	"No",
			"true"	=>	"Yes \(/Gy\)",
		},
		"EnableEnhancedInstructionSet"		=>
		{
			"0"	=>	"Not Set",
			"1"	=>	"Streaming SIMD Extensions \(\/arch:SSE\)",
			"2"	=>	"Streaming SIMD Extensions 2 \(\/arch:SSE2\)",
		},
		"FloatingPointModel"			=>
		{
			"0"	=>	"Precise \(\/fp:precise\)",
			"1"	=>	"Strict \(\/fp:strict\)",
			"2"	=>	"Fast \(\/fp:fast\)",
		},
		"FloatingPointExceptions"		=>
		{
			"false"	=>	"No",
			"true"	=>	"Yes \(\/fp:except\)",
		},

		# Language
		"DisableLanguageExtensions"		=>
		{
			"false"	=>	"No",
			"true"	=>	"Yes \(/Za\)",
		},
		"DefaultCharIsUnsigned"			=>
		{
			"false"	=>	"No",
			"true"	=>	"Yes \(\/J\)",
		},
		"TreatWChar_tAsBuiltInType"		=>
		{
			"false"	=>	"No \(\/Zc:wchar_t-\)",
			"true"	=>	"Yes",
		},
		"ForceConformanceInForLoopScope"	=>
		{
			"false"	=>	"No \(\/Zc:forScope-\)",
			"true"	=>	"Yes",
		},
		"RuntimeTypeInfo"			=>
		{
			"false"	=>	"No \(\/GR-\)",
			"true"	=>	"Yes",
		},
		"OpenMP"			=>
		{
			"false"	=>	"No",
			"true"	=>	"Yes \(\/openmp\)",
		},

		# Precompiled Headers
		"UsePrecompiledHeader"			=>
		{
			"0"	=>	"Not Using Precompiled Headers",
			"1"	=>	"Create Precompiled Header \(\/Yc\)",
			"2"	=>	"Use Precompiled Header \(\/Yu\)",
		},

		# Output Files
		"ExpandAttributedSource"		=>
		{
			"false"	=>	"No",
			"true"	=>	"Yes \(\/Fx\)",
		},
		"AssemblerOutput"			=>
		{
			"0"	=>	"No Listing",
			"1"	=>	"Assembly-Only Listing \(\/FA\)",
			"2"	=>	"Assembly, Machine Code and Source \(\/FAcs\)",
			"3"	=>	"Assembly With Machine Code \(\/FAc\)",
			"4"	=>	"Assembly With Source Code \(\/FAs\)",
		},
		"GenerateXMLDocumentationFiles"		=>
		{
			"false"	=>	"No",
			"true"	=>	"Yes (/doc)",
		},

		# Browse Information
		"BrowseInformation"		=>
		{
			"0"	=>	"None",
			"1"	=>	"Include All Browse Information \(\/FR\)",
			"2"	=>	"No Local Symbols \(\/Fr\)",
		},

		# Advanced
		"CallingConvention"			=>
		{
			"0"	=>	"__cdecl \(\/Gd\)",
			"1"	=>	"__fastcall \(\/Gr\)",
			"2"	=>	"__stdcall \(\/Gz\)",
		},
		"CompileAs"				=>
		{
			"0"	=>	"Default",
			"1"	=>	"Compile as C Code \(\/TC\)",
			"2"	=>	"Compile as C\+\+ Code \(\/TP\)",
		},
		"ShowIncludes"	=>
		{
			"false"	=>	"No",
			"true"	=>	"Yes \(\/showIncludes\)",
		},
		"UndefineAllPreprocessorDefinitions"	=>
		{
			"false"	=>	"No",
			"true"	=>	"Yes \(\/u\)",
		},
		"UseFullPaths"				=>
		{
			"false"	=>	"No",
			"true"	=>	"Yes \(\/FC\)",
		},
		"OmitDefaultLibName"			=>
		{
			"false"	=>	"No",
			"true"	=>	"Yes \(\/Zl\)",
		},
		"ErrorReporting"			=>
		{
			"0"	=>	"Default",
			"1"	=>	"Prompt Immediately \(\/errorReport:prompt\)",
			"2"	=>	"Queue For Next Login \(\/errorReport:queue\)",
		},

	# Librarian
		# General
		"SuppressStartupBanner"			=>
		{
			"false"	=>	"No",
			"true"	=>	"Yes \(\/NOLOGO\)",
		},
		"IgnoreAllDefaultLibraries"		=>
		{
			"false"	=>	"No",
			"true"	=>	"Yes \(\/NODEFAULTLIB\)",
		},
		"UseUnicodeResponseFiles"		=>
		{
			"false"	=>	"No",
			"true"	=>	"Yes",
		},
		"LinkLibraryDependencies"		=>
		{
			"false"	=>	"No",
			"true"	=>	"Yes",
		},

	# Linker
		#General
		"ShowProgress"				=>
		{
			"0"	=>	"Not Set",
			"1"	=>	"Display All Progress Messages \(\/VERBOSE\)",
			"2"	=>	"Displays Some Progress Messages \(\/VERBOSE:LIB\)",
		},
		"LinkIncremental"			=>
		{
			"0"	=>	"Default",
			"1"	=>	"No \(\/INCREMENTAL:NO\)",
			"2"	=>	"Yes \(\/INCREMENTAL\)",
		},
		"SuppressStartupBanner"			=>
		{
			"false"	=>	"No",
			"true"	=>	"Yes \(\/NOLOGO\)",
		},
		"IgnoreImportLibrary"			=>
		{
			"false"	=>	"No",
			"true"	=>	"Yes",
		},
		"RegisterOutput"			=>
		{
			"false"	=>	"No",
			"true"	=>	"Yes",
		},
		"LinkLibraryDependencies"		=>
		{
			"false"	=>	"No",
			"true"	=>	"Yes",
		},
		"UseLibraryDependencyInputs"		=>
		{
			"false"	=>	"No",
			"true"	=>	"Yes",
		},
		"UseUnicodeResponseFiles"		=>
		{
			"false"	=>	"No",
			"true"	=>	"Yes",
		},

		# Input
		"IgnoreAllDefaultLibraries"		=>
		{
			"false"	=>	"No",
			"true"	=>	"Yes \(\/NODEFAULTLIB\)",
		},

		# Manifest File
		"GenerateManifest"			=>
		{
			"false"	=>	"No",
			"true"	=>	"Yes",
		},
		"AllowIsolation"			=>
		{
			"false"	=>	"Don't allow side-by-side isolation \(\/ALLOWISOLATION:NO\)",
			"true"	=>	"Yes",
		},


		# Debugging
		"GenerateDebugInformation"		=>
		{
			"false"	=>	"No",
			"true"	=>	"Yes \(\/DEBUG\)",
		},
		"GenerateMapFile"			=>
		{
			"false"	=>	"No",
			"true"	=>	"Yes \(\/MAP\)",
		},
		"MapExports"				=>
		{
			"false"	=>	"No",
			"true"	=>	"Yes \(\/MAPINFO:EXPORTS\)",
		},
		"AssemblyDebug"				=>
		{
			"0"	=>	"No Debuggable attribute emitted",
			"1"	=>	"Runtime tracking and disable optimizations \(\/ASSEMBLYDEBUG\)",
			"2"	=>	"No runtime tracking and enable optimizations \(\/ASSEMBLYDEBUG:DISABLE\)",
		},		

		# System
		"SubSystem"				=>
		{
			"0"	=>	"Not Set",
			"1"	=>	"Console \(\/SUBSYSTEM:CONSOLE\)",
			"2"	=>	"Windows \(\/SUBSYSTEM:WINDOWS\)",
			"3"	=>	"Native \(\/SUBSYSTEM:NATIVE\)",
			"4"	=>	"EFI Application \(\/SUBSYSTEM:EFI_APPLICATION\)",
			"5"	=>	"EFI Boot Service Driver \(\/SUBSYSTEM:EFI_BOOT_SERVICE_DRIVER\)",
			"6"	=>	"EFI ROM \(\/SUBSYSTEM:EFI_ROM\)",
			"7"	=>	"EFI Runtime \(\/SUBSYSTEM:EFI_RUNTIME_DRIVER\)",
			"8"	=>	"Posix \(\/SUBSYSTEM:POSIX\)",
			"9"	=>	"WindowsCE \(\/SUBSYSTEM:WINDOWSCE\)",
		},
		"LargeAddressesAware"			=>
		{
			"0"	=>	"Default",
			"1"	=>	"Do Not Support Addresses Larger Than 2 Gigabytes \(\/LARGEADDRESSAWARE:NO\)",
			"2"	=>	"Support Addresses Larger Than 2 Gigabytes \(\/LARGEADDRESSAWARE\)",
		},
		"TerminalServerAware"			=>
		{
			"0"	=>	"Default",
			"1"	=>	"Not Terminal Server Aware \(\/TSAWARE:NO\)",
			"2"	=>	"Application is Terminal Server Aware \(\/TSAWARE\)",
		},
		"SwapRunFromCD"				=>
		{
			"false"	=>	"No",
			"true"	=>	"Yes \(\/SWAPRUN:CD\)",
		},
		"SwapRunFromNet"			=>
		{
			"false"	=>	"No",
			"true"	=>	"Yes \(\/SWAPRUN:NET\)",
		},
		"Driver"			=>
		{
			"0"	=>	"Not Set",
			"1"	=>	"Driver \(\/DRIVER\)",
			"2"	=>	"Up Only \(\/DRIVER:UPONLY\)",
			"3"	=>	"WDM \(\/DRIVER:WDM\)",
		},

		#Optimization
		"OptimizeReferences"			=>
		{
			"0"	=>	"Default",
			"1"	=>	"Keep Unreferenced Data \(\/OPT:NOREF\)",
			"2"	=>	"Eliminate Unreferenced Data \(\/OPT:REF\)",
		},
		"EnableCOMDATFolding"			=>
		{
			"0"	=>	"Default",
			"1"	=>	"Do Not Remove Redundant COMDATs \(\/OPT:NOICF\)",
			"2"	=>	"Remove Redundant COMDATs \(\/OPT:ICF\)",
		},
		"OptimizeForWindows98"			=>
		{
			"0"	=>	"Default",
			"1"	=>	"No \(\/OPT:NOWIN98\)",
			"2"	=>	"Yes \(\/OPT:WIN98\)",
		},
		"LinkTimeCodeGeneration"		=>
		{
			"0"	=>	"Default",
			"1"	=>	"Use Link Time Code Generation \(\/ltcg\)",
			"2"	=>	"Profile Guided Optimization - Instrument \(\/ltcg:pginstrument\)",
			"3"	=>	"Profile Guided Optimization - Optimize \(\/ltcg:pgoptimize\)",
			"4"	=>	"Profile Guided Optimization - Update \(\/ltcg:pgupdate\)",
		},

		# Embedded IDL
		"IgnoreEmbeddedIDL"			=>
		{
			"false"	=>	"No",
			"true"	=>	"Yes \(\/IGNOREIDL\)",
		},

		#Advanced
		"ResourceOnlyDLL"			=>
		{
			"false"	=>	"No",
			"true"	=>	"Yes \(\/NOENTRY\)",
		},
		"SetChecksum"				=>
		{
			"false"	=>	"No",
			"true"	=>	"Yes \(\/RELEASE\)",
		},
		"FixedBaseAddress"			=>
		{
			"0"	=>	"Default",
			"1"	=>	"Generate a relocation section \(\/FIXED:NO\)",
			"2"	=>	"Image must be loaded at a fixed address \(\/FIXED\)",
		},
		"TurnOffAssemblyGeneration"		=>
		{
			"false"	=>	"No",
			"true"	=>	"Yes \(\/NOASSEMBLY\)",
		},
		"SupportUnloadOfDelayLoadedDLL"		=>
		{
			"0"	=>	"Don't Support Unload",
			"1"	=>	"Support Unload \(\/DELAY:UNLOAD\)",
		},
		"TargetMachine"				=>
		{
			"0"	=>	"Not Set",
			"1"	=>	"MachineX86 \(\/MACHINE:X86\)",
			"2"	=>	"MachineAM33 \(\/MACHINE:AM33\)",
			"3"	=>	"MachineARM \(\/MACHINE:ARM\)",
			"4"	=>	"MachineEBC \(\/MACHINE:EBC\)",
			"5"	=>	"MachineIA64 \(\/MACHINE:IA64\)",
			"6"	=>	"MachineM32R \(\/MACHINE:M32R\)",
			"7"	=>	"MachineMIPS \(\/MACHINE:MIPS\)",
			"8"	=>	"MachineMIPS16 \(\/MACHINE:MIPS16\)",
			"9"	=>	"MachineMIPSFPU \(\/MACHINE:MIPSFPU\)",
			"10"	=>	"MachineMIPSFPU16 \(\/MACHINE:MIPSFPU16\)",
			"11"	=>	"MachineMIPSR41XX \(\/MACHINE:MIPSR41XX\)",
			"12"	=>	"MachineSH3 \(\/MACHINE:SH3\)",
			"13"	=>	"MachineSH4 \(\/MACHINE:SH4\)",
			"14"	=>	"MachineSH5 \(\/MACHINE:SH5\)",
			"15"	=>	"MachineTHUMB \(\/MACHINE:THUMB\)",
			"16"	=>	"MachineX64 \(\/MACHINE:X64\)",
		},
		"Profile"				=>
		{
			"false"	=>	"No",
			"true"	=>	"Enable Profiling information \(\/PROFILE\)",
		},
		"CLRThreadAttribute"			=>
		{
			"0"	=>	"No threading attribute set",
			"1"	=>	"MTA threading attribute \(\/CLRTHREADATTRIBUTE:MTA\)",
			"2"	=>	"STA threading attribute \(\/CLRTHREADATTRIBUTE:STA\)",
		},
		"CLRImageType"				=>
		{
			"0"	=>	"Default image type",
			"1"	=>	"Force IJW image \(\/CLRIMAGETYPE:IJW\)",
			"2"	=>	"Force pure IL image \(\/CLRIMAGETYPE:PURE\)",
			"3"	=>	"Force safe IL image \(\/CLRIMAGETYPE:SAFE\)",
		},
		"DelaySign"				=>
		{
			"false"	=>	"No",
			"true"	=>	"Yes \(\/DELAYSIGN\)",
		},
		"ErrorReporting"			=>
		{
			"0"	=>	"Default",
			"1"	=>	"Prompt Immediately \(\/ERRORREPORT:PROMPT\)",
			"2"	=>	"Queue For Next Login \(\/ERRORREPORT:QUEUE\)",
		},
		"CLRUnmanagedCodeCheck"			=>
		{
			"false"	=>	"No",
			"true"	=>	"Apply Unmanaged Code Check \(\/CLRUNMANAGEDCODECHECK\)",
		},

	# Resources
		# General
		"Culture"				=>
		{
			"1000"	=>	"Default",
			"1001"	=>	"Afrikaans \(0x436\)",
			"1002"	=>	"Albanian \(0x41c\)",
			"1003"	=>	"Arabic (Saudi Arabia) \(0x401\)",
			"1004"	=>	"Arabic (Iraq) \(0x801\)",
			"1005"	=>	"Arabic (Egypt) \(0xc01\)",
			"1006"	=>	"Arabic (Libya) \(0x1001\)",
			"1007"	=>	"Arabic (Algeria) \(0x1401\)",
			"1008"	=>	"Arabic (Morocco) \(0x1801\)",
			"1009"	=>	"Arabic (Tunisia) \(0x1c01\)",
			"1010"	=>	"Arabic (Oman) \(0x2001\)",
			"1011"	=>	"Arabic (Yemen) \(0x2401\)",
			"1012"	=>	"Arabic (Syria) \(0x2801\)",
			"1013"	=>	"Arabic (Jordan) \(0x2c01\)",
			"1014"	=>	"Arabic (Lebanon) \(0x3001\)",
			"1015"	=>	"Arabic (Kuwait) \(0x3401\)",
			"1016"	=>	"Arabic (U.A.E.) \(0x3801\)",
			"1017"	=>	"Arabic (Bahrain) \(0x3c01\)",
			"1018"	=>	"Arabic (Qatar) \(0x4001\)",
			"1019"	=>	"Basque \(0x42d\)",
			"1020"	=>	"Bulgarian \(0x402\)",
			"1021"	=>	"Belarusian \(0x423\)",
			"1022"	=>	"Catalan \(0x403\)",
			"1023"	=>	"Chinese (Taiwan) \(0x404\)",
			"1024"	=>	"Chinese (PRC) \(0x804\)",
			"1025"	=>	"Chinese (Hong Kong S.A.R.) \(0xc04\)",
			"1026"	=>	"Chinese (Singapore) \(0x1004\)",
			"1027"	=>	"Croatian \(0x41a\)",
			"1028"	=>	"Czech \(0x405\)",
			"1029"	=>	"Danish \(0x406\)",
			"1030"	=>	"Dutch (Netherlands) \(0x413\)",
			"1031"	=>	"Dutch (Belgium) \(0x813\)",
			"1032"	=>	"English (United States) \(0x409\)",
			"1033"	=>	"English (United Kingdom) \(0x809\)",
			"1034"	=>	"English (Australia) \(0xc09\)",
			"1035"	=>	"English (Canada) \(0x1009\)",
			"1036"	=>	"English (New Zealand) \(0x1409\)",
			"1037"	=>	"English (Ireland) \(0x1809\)",
			"1038"	=>	"English (South Africa) \(0x1c09\)",
			"1039"	=>	"English (Jamaica) \(0x2009\)",
			"1040"	=>	"English (Caribbean) \(0x2409\)",
			"1041"	=>	"Estonian \(0x425\)",
			"1042"	=>	"Farsi \(0x429\)",
			"1043"	=>	"Finnish \(0x40b)",
			"1044"	=>	"French (France) \(0x40c\)",
			"1045"	=>	"French (Belgium) \(0x80c\)",
			"1046"	=>	"French (Canada) \(0xc0c\)",
			"1047"	=>	"French (Switzerland) \(0x100c\)",
			"1048"	=>	"French (Luxembourg) \(0x140c\)",
			"1049"	=>	"German (Germany) \(0x407\)",
			"1050"	=>	"German (Switzerland) \(0x807\)",
			"1051"	=>	"German (Austria) \(0xc07\)",
			"1052"	=>	"German (Luxembourg) \(0x1007\)",
			"1053"	=>	"German (Liechtenstein) \(0x1407\)",
			"1054"	=>	"Greek \(0x408\)",
			"1055"	=>	"Hebrew \(0x40d\)",
			"1056"	=>	"Hungarian \(0x40e\)",
			"1057"	=>	"Icelandic \(0x40f\)",
			"1058"	=>	"Indonesian \(0x421\)",
			"1059"	=>	"Italian (Italy) \(0x410\)",
			"1060"	=>	"Italian (Switzerland) \(0x810\)",
			"1061"	=>	"Japanese \(0x411\)",
			"1062"	=>	"Korean \(0x412\)",
			"1063"	=>	"0x812",
			"1064"	=>	"Latvian \(0x426\)",
			"1065"	=>	"Lithuanian \(0x427\)",
			"1066"	=>	"Norwegian (Bokmal) \(0x414\)",
			"1067"	=>	"Norwegian (Nynorsk) \(0x814\)",
			"1068"	=>	"Polish \(0x415\)",
			"1069"	=>	"Portuguese (Brazil) \(0x416\)",
			"1070"	=>	"Portuguese (Portugal) \(0x816\)",
			"1071"	=>	"Romanian \(0x418\)",
			"1072"	=>	"Russian \(0x419\)",
			"1073"	=>	"Slovak \(0x41b\)",
			"1074"	=>	"Spanish (Traditional Sort) \(0x40a\)",
			"1075"	=>	"Spanish (Mexico) \(0x80a\)",
			"1076"	=>	"Spanish (International Sort) \(0xc0a\)",
			"1077"	=>	"Spanish (Guatemala) \(0x100a\)",
			"1078"	=>	"Spanish (Costa Rica) \(0x140a\)",
			"1079"	=>	"Spanish (Panama) \(0x180a\)",
			"1080"	=>	"Spanish (Dominican Republic) \(0x1c0a\)",
			"1081"	=>	"Spanish (Venezuela) \(0x200a\)",
			"1082"	=>	"Spanish (Colombia) \(0x240a\)",
			"1083"	=>	"Spanish (Peru) \(0x280a\)",
			"1084"	=>	"Spanish (Argentina) \(0x2c0a\)",
			"1085"	=>	"Spanish (Ecuador) \(0x300a\)",
			"1086"	=>	"Spanish (Chile) \(0x340a\)",
			"1087"	=>	"Spanish (Uruguay) \(0x380a\)",
			"1088"	=>	"Spanish (Paraguay) \(0x3c0a\)",
			"1089"	=>	"Spanish (Bolivia) \(0x400a\)",
			"1090"	=>	"Swedish \(0x41d\)",
			"1091"	=>	"Thai \(0x41e\)",
			"1092"	=>	"Turkish \(0x41f\)",
			"1093"	=>	"Ukrainian \(0x422\)",
			"1094"	=>	"Serbian (Latin) \(0x81a\)",
			"1095"	=>	"Urdu \(0x420\)",
		},
		"IgnoreStandardIncludePath"		=>	"IgnoreStandardIncludePath",
		{
			"1"	=>	"No",
			"2"	=>	"Yes \(\/X\)",
		},
		"ShowProgress"				=>	"ShowProgress",
		{
			"1"	=>	"No",
			"2"	=>	"Yes \(\/v\)",
		},
	);


# Hash matches vcproj configuration names with their output versions
my(%configurationNames) = (	"Base"		=>	"base",
				"DoD"		=>	"dod",
				"CounterStrike"	=>	"cstrike",
				"HL1"		=>	"hl1",
				"HL2"		=>	"hl2",
				"Episodic HL2"	=>	"episodic",
				"TF"		=>	"tf",
				"SDK"		=>	"sdk",
				"HL2MP"		=>	"hl2mp",
				"LostCoast"	=>	"lostcoast",
				"Portal"	=>	"portal",
				"Dedicated"	=>	"dedicated",
	    	      );
my(@configurations) = keys %configurationNames;

my(%toolNames) = (
		"VCCLCompilerTool"		=>	"Compiler",
		"VCCustomBuildTool"		=>	"CustomBuildStep",
		"VCLinkerTool"			=>	"Linker",
		"VCPostBuildEventTool"		=>	"PostBuildEvent",
		"VCPreBuildEventTool"		=>	"PreBuildEvent",
		"VCPreLinkEventTool"		=>	"PreLinkEvent",
		"VCResourceCompilerTool"	=>	"Resources",
		"VCLibrarianTool"		=>	"Librarian",
		);
my(@tools) = values %toolNames;

my($baseConfiguration) = "Application (.exe)";
my(%baseConfigurationTypes) = (
		"Dynamic Library (.dll)"	=>	"dll",
		"Application (.exe)"		=>	"exe",
		"Static Library (.lib)"		=>	"lib",
		"Utility"			=>	"lib",
		);

my($configurationSubsystem) = "con";

my(%outputs);
my(%excludes);
my(%filesAdded);

for ( @configurations )
{
	$outputs{$_} = ();
	$excludes{$_} = 0;
	$filesAdded{$_} = 0;
}

my(%keytabs) = (	0	=>	"\t\t\t\t\t\t\t\t\t\t",
			4	=>	"\t\t\t\t\t\t\t\t\t",
			8	=>	"\t\t\t\t\t\t\t\t",
			12	=>	"\t\t\t\t\t\t\t",
			16	=>	"\t\t\t\t\t\t",
			20	=>	"\t\t\t\t\t",
			24	=>	"\t\t\t\t",
			28	=>	"\t\t\t",
			32	=>	"\t\t",
			36	=>	"\t",
		);

######################################################
# Subroutines
######################################################

sub break
{
	return;
}

sub outputToAllConfigurations
{
	for ( @configurations )
	{
		push( @{ $outputs{$_} }, @lineToPrint );
	}
}


sub compare_arrays 
{
	my ($first, $second) = @_;
	return 0 unless @$first == @$second;
	for (my $i = 0; $i < @$first; $i++) 
	{
		if ( $first->[$i] =~ /^(Debug|Release)$/ )
		{
			next;
		} 
		return 0 if $first->[$i] ne $second->[$i];
	}
	return 1;
}


sub set_current_configuration
{
	my($line) = shift;

	$line =~ ( /Name="(\w+) ([\w\s]*)\|/ );
	my($name1) = $1;
	my($name2) = $2;
	if ( $name2 =~ /Release|Debug/ )
	{
		$currentConfig = $name1;
	}
	else
	{
		$currentConfig = $name2;
	}

	if ( $line =~ ( /Name="(Release|Debug)\|/ ) )
	{
		# default configurations
		$currentConfig = "Base";
	}
}

####################################################
sub processFileConfig
{
	$spaceBeforeFile = 1;

	my($splitFiles) = 0;
	my($line) = $_[++$lineCt];

	# Set the current configuration

	set_current_configuration( $line );

	push( @tabs, "\t" );

	$line =~ ( /Name="([\w\s]*)\|/ );
	my($configName) = ( $1 =~ /(Debug|Release)/ );
	push( @{ $configOutput{$currentConfig} }, @tabs, "\$Configuration\t\"", $configName, "\"\n" );
	push( @{ $configOutput{$currentConfig} }, @tabs, "\{\n" );

	# Process the configuration

	my($configResult) = processConfiguration( @_ );

	# end if this configuration
	push( @{ $configOutput{$currentConfig} }, @tabs, "\}\n" );

	if ( $configResult == 1 )
	{
		# Mark this file as excluded for the current configuration
				
		$excludes{$currentConfig} = 1;
		$splitFiles = 1;
	}
	elsif ( $configResult == -1 )
	{
		# Configuration is empty, so clear it

		@{ $configOutput{$currentConfig} } = ();
	}

	pop( @tabs );
	return $splitFiles;
}


####################################################
sub processFile
{
	# get the file name and path

	@lineToPrint = ();
	if ( $spaceBeforeFile == 1 )
	{
		$spaceBeforeFile = 0;
#		push( @lineToPrint, "\n" );
	}

	$_[++$lineCt] =~ ( /RelativePath="([^"]+)"/ );
	my $line = $1;

	# replace ..\ and .\ in filenames
	$line =~ s/^\Q$srcdirBackslash\E\\/\$SRCDIR\\/;
	$line =~ s/^\Q$srcdirForwardslash\E\//\$SRCDIR\\/;
	$line =~ s/^\.\\//;

	push( @lineToPrint, @tabs, "\t\$File\t\"", $line, "\"\n" );

	push( @tabs, "\t" );

	my($splitFiles) = 0;
	my($configFound) = 0;

	# loop until the </File> tag

	%configOutput = ();
	while ( $_[++$lineCt] !~ /^\s*\<\/File\>$/ )
	{	
		# Check for file specific configurations
		if ( $_[$lineCt] =~ /^\s*\<FileConfiguration/ )
		{
			$configFound = 1;
			$splitFiles += processFileConfig( @_ );
		}
	}
	
	# Compare the configurations to see if the files should be split
	if ( $configFound && !$splitFiles )
	{
		for ( @configurations )
		{
			if ( $_ eq "Base" || $_ eq "Dedicated" )
			{
				next;
			}
			if ( !compare_arrays( \@{ $configOutput{"HL2"} }, \@{ $configOutput{$_} } ) )
			{
				$splitFiles = 1;
				last;
			}
		}
		if ( !$splitFiles )
		{
			push( @{ $configOutput{"Base"} }, @{ $configOutput{"HL2"} } );
		}
	}
	
	# Add the file and configuration to the appropriate projects
	
	if ( !$splitFiles )
	{
		if ( @{ $configOutput{"Base"} } > 0 )
		{
			push( @{ $outputs{"Base"} }, @lineToPrint, @tabs, "\{\n" );
			push( @{ $outputs{"Base"} }, @{ $configOutput{"Base"} } );
			push( @{ $outputs{"Base"} }, @tabs, "\}\n\n" );
		}
		else
		{
			push( @{ $outputs{"Base"} }, @lineToPrint );
		}
		$filesAdded{"Base"} = 1;
	}
	else
	{
		$excludes{"Base"} = 1;
		$exclusionsFound = 1;		
		for ( @configurations )
		{
			if ( !$excludes{$_} )
			{
				if ( @{ $configOutput{$_} } > 0 )
				{
					push( @{ $outputs{$_} }, @lineToPrint, @tabs, "\{\n" );
					push( @{ $outputs{$_} }, @{ $configOutput{$_} } );
					push( @{ $outputs{$_} }, @tabs, "\}\n\n" );
				}
				else
				{
					push( @{ $outputs{$_} }, @lineToPrint );
				}
				$filesAdded{$_} = 1;
			}
			else
			{
				# reset the exclude flag
				$excludes{$_} = 0;
			}
		}
	}

	pop( @tabs );
}


####################################################
sub processFolder
{
	push( @tabs, "\t" );

	# Grab the folder name and add it to all configuration's outputs

	$_[++$lineCt] =~ ( /Name="([^"]+)"/ );

	@lineToPrint = ( @tabs, "\$Folder\t\"", $1, "\"\n", @tabs, "\{\n" );
	outputToAllConfigurations();

	# Loop until the </Filter> tag

	while ( $_[++$lineCt] !~ /^\s*\<\/Filter\>$/ )
	{
		if ( $_[$lineCt] =~ /^\s*\<Filter$/ )
		{
			# Start of a new folder

			if ( $spaceBeforeFolder == 1 )
			{
				$spaceBeforeFolder = 0;
				@lineToPrint = "\n";
				outputToAllConfigurations();
			}
			processFolder( @_ );
		}
		elsif ( $_[$lineCt] =~ /^\s*\<File$/ )
		{
			# Start of a new file

			processFile( @_ );
			$spaceBeforeFolder = 1;
		}
	}
	
	# End of the folder
		
	@lineToPrint = ( @tabs, "\}\n" );
	for ( @configurations )
	{
		push( @{ $outputs{$_} }, @lineToPrint );
	}
	pop( @tabs );
}


####################################################
sub processConfigOption
{
	my($line) = shift;
	my($tool) = shift;

	# Get the keyname and value
	if ( $line !~ /(\w+)="([^"\n]*)(.*)/ )
	{
		return;
	}

	my($keyname) 	= $1;
	my($keyvalue) 	= $2;
	my($lastChar)	= $3;
	my($outputvalue);

	# Lookup the keyname
	if ( $outputvalue = $configOptionsSingleValue{$tool}{$keyname} )
	{
		# A single value option - outputvalue is the VPC defined keyname
		
		# Translate true/false to Yes/No
		$keyvalue =~ s/true/Yes/;
		$keyvalue =~ s/false/No/;

		$keyname = $outputvalue;
	}
	elsif ( $outputvalue = $configOptionValues{$keyname}{$keyvalue} )
	{
		# A multi-value option - outputvalue is the desired option setting in string form

		my($translatedName) = $configOptionsMultiValue{$keyname};

		# Do some bookkeeping for later
		if ( $translatedName =~ /^SubSystem$/ && $outputvalue =~ /Windows/ )
		{
			$configurationSubsystem = "win";
		}
		elsif ( $translatedName =~ /^ConfigurationType$/ )
		{
			$baseConfiguration = $outputvalue;
		}

		if ( $usestring )
		{
			return $translatedName, $outputvalue;
		}
		else
		{
			return $translatedName, $keyvalue;
		}
	}
	else
	{
		# For debugging
		print( "Line ", $lineCt, ": Error, no config found for Tool: ", $tool, ", ", $keyname, "=", $keyvalue, "\n" );
#		push( @tempOutput, "\/\/" );
	}

	# special handling for multi-line options
	if ( !$lastChar )
	{
		my($nextline) = ( @lines[++$lineCt] =~ /([^\n]*)/ );
		while( $nextline !~ /\"/ )
		{
			$keyvalue = join( '', $keyvalue, "\" \\ \"\\n\"\n", @tabs, $keytabs{0}, "\"", $nextline );
			($nextline) = ( @lines[++$lineCt] =~ /([^\n]*)/ );
		}
	}

	# replace &quot; and ..\ and .\ in option values
	$keyvalue =~ s/(?<=[^\\])\Q$srcdirBackslash\E\\/\$SRCDIR\\/g;
	$keyvalue =~ s/(?<=[^\/])\Q$srcdirForwardslash\E\//\$SRCDIR\\/g;
	$keyvalue =~ s/^\Q$srcdirBackslash\E\\/\$SRCDIR\\/g;
	$keyvalue =~ s/^\Q$srcdirForwardslash\E\//\$SRCDIR\\/g;
	$keyvalue =~ s/^\.\\//g;
	$keyvalue =~ s/\&quot\;/\$QUOTE/g;

	return $keyname, $keyvalue;

}


####################################################
sub processBuildTool
{	
	push( @tabs, "\t" );
	# Grab the tool name

	$_[++$lineCt] =~ ( /Name="([^"]+)"/ );
	my($toolName) = $1;

	if ( !$toolNames{$toolName} )
	{
		pop( @tabs );
		while ( $_[$lineCt] !~ /\/\>$/ ) 
		{
			++$lineCt;
		}
		return;
	}

	my( @tempOutput );

	if ( !$parsingFiles )
	{
		push( @tempOutput, "\n" );
	}
	push( @tempOutput, @tabs, "\$", $toolNames{$toolName}, "\n" );
	push( @tempOutput, @tabs, "\{\n" );

	# Loop until the /> tag

	my($keyname);
	my($keyvalue);
	my($optionsFound) = 0;
	while ( $_[$lineCt] !~ /\/\>$/ )
	{
		($keyname, $keyvalue) = processConfigOption( $_[++$lineCt], $toolName );

		if ( $keyname )
		{
			if ( $keyvalue || !$stripEmptyOptions )
			{
				$optionsFound = 1;
			}
			my($len) = $tabstop * int( (length( $keyname ) + $tabstop + 1) / $tabstop );
			push( @tempOutput, @tabs, "\t\$", $keyname, $keytabs{$len}, "\"", $keyvalue, "\"\n" );
		}
	}

	pop( @tabs );
	
	# End of the tool
	if ( $optionsFound )
	{
		push( @{ $configOutput{$currentConfig} }, @tempOutput, @tabs, "\t}\n" );
		return 1;
	}
	return 0;
}


####################################################
sub processConfiguration
{	
	my( $configOptionsFound ) = 0;
	my( $startedGeneral ) = 0;

	# Loop until the </Configuration> tag

	while ( $_[++$lineCt] !~ /^\s*\<\/(File)*Configuration\>$/ )
	{
		if ( $_[$lineCt] =~ /^\s*\<Tool$/ )
		{
			if ( $startedGeneral )
			{
				# close out the faked "General" category
				$startedGeneral = 0;
				push( @{ $configOutput{$currentConfig} }, @tabs, "\}\n" );
				pop( @tabs );
			}

			# Start of a new build tool

			$configOptionsFound += processBuildTool( @_ );
		}
		elsif ( $_[$lineCt] =~ /=/ )
		{
			# Process the main configuration properties

			if ( !$startedGeneral && !$parsingFiles )
			{
				# start the fake "General" category
				$startedGeneral = 1;
				
				push( @tabs, "\t" );
				push( @{ $configOutput{$currentConfig} }, @tabs, "\$General\n" );
				push( @{ $configOutput{$currentConfig} }, @tabs, "\{\n" );
			}

			my($keyname);
			my($keyvalue);
			($keyname, $keyvalue) = processConfigOption( $_[$lineCt], "General" );

			if ( $keyname )
			{
				# Handle some special cases
				if ( $keyname =~ /^ExcludedFromBuild$/ && $keyvalue =~ /^Yes$/ )
				{
					@{ $configOutput{$currentConfig} } = ();
					return 1;
				}

				++$configOptionsFound;
				my($len) = $tabstop * int( (length( $keyname ) + $tabstop + 1) / $tabstop );
				push( @{ $configOutput{$currentConfig} }, @tabs, "\t\$", $keyname, $keytabs{$len}, "\"", $keyvalue, "\"\n" );
			}
		}
	}

	# See if any config options were recorded
	if ( !$configOptionsFound )
	{
		return -1;
	}

	return 0;
}


######################################################
# Code section
######################################################

if ( !$ARGV[0] )
{
	print( "Error: no project name specified\n" );
	exit;
}

# Read in the source file

my $infile = $ARGV[0];
$infile =~ s/.vcproj//;
open(INFILE, "$infile.vcproj" );

@lines = <INFILE>;
close( INFILE );

my(@pathArray) = split(/\\/, $infile );
my($vcprojname) = $pathArray[$#pathArray];

unless ( $outputPath )
{
	$outputPath = $infile;
	$outputPath =~ s/$vcprojname//;
}

# build the fileheader
my(@fileheader);
push( @fileheader, "\/\/-----------------------------------------------------------------------------\n" );
push( @fileheader, "\/\/\t",uc($vcprojname),".VPC\n" );
push( @fileheader, "\/\/\n" );
push( @fileheader, "\/\/\tProject Script\n" );
push( @fileheader, "\/\/-----------------------------------------------------------------------------\n" );
push( @fileheader, "\/\/ ***** AUTO-GENERATED: PLEASE FIXUP MANUALLY BEFORE USING THIS SCRIPT! *****\n" );
push( @fileheader, "\n" );

push( @fileheader, "\$Macro SRCDIR\t\t\".." );
$srcdirBackslash = "..";
my($dirct) = $#pathArray - 2;
for ( my($i) = 0; $i < $dirct; ++$i )
{
	push( @fileheader, "\\.." );
	$srcdirBackslash = join( "\\", $srcdirBackslash, ".." );
}
$srcdirForwardslash = $srcdirBackslash;
$srcdirForwardslash =~ s/\\/\//g;
push( @fileheader, "\"\n" );


# Process the file one line at a time

my $folderOpenBrace = 0;
for( $lineCt = 0; $lineCt < @lines; ++$lineCt ) 
{
	my($line) = @lines[$lineCt];

	if ( !$nameFound && $line =~ ( /Name="([^"]+)"/ ) )
	{
		$projectName = $1;

		# Print the project name
		
		my $capName = $projectName;
		$capName =~ s/\b(\w)/\U$1/g;
		@lineToPrint = ( "\$Project \"", $capName, "\"\n\{\n" );
		for ( @configurations )
		{
			push( @{ $outputs{$_} }, @lineToPrint );
		}
		$nameFound = 1;
		$folderOpenBrace = 1;

		# Clean up the directory

#		for ( @configurations )
#		{
#			# delete the existing files
#			my($filename) = join('_', $vcprojname, $configurationNames{$_} );
#			if( $outputPath )
#			{
#				$filename = join('/', $outputPath, $filename );
#				$filename =~ s/\//\\/g;
#			}
#			system( "del /Q $filename.vpc" );
#		}
	}

	if ( $line =~ /^\s*\<Configuration$/ )
	{
		# Start of a new configuration

		# Get the configuration name and then process the configuration

		my($configLine) = @lines[++$lineCt];
		set_current_configuration( $configLine );

 		$configLine =~ ( /Name="([\w\s]*)\|/ );
		my($configName) = ( $1 =~ /(Debug|Release)/ );
		$currentConfig = join( '_', $currentConfig, $configName );

		push( @{ $configOutput{$currentConfig} }, "\$Configuration\t\"", $configName, "\"\n" );
		push( @{ $configOutput{$currentConfig} }, "\{\n" );

		processConfiguration( @lines );


		# end if this configuration
		push( @{ $configOutput{$currentConfig} }, "\}\n\n" );
	}
	elsif ( $line =~ /^\s*\<Files\>$/ )
	{
		# end of configurations section - write out the files

		# first, finish filling in the header info

		my($configtype) = $baseConfigurationTypes{$baseConfiguration};
		if ( $configtype =~ /lib/ )
		{
			push( @fileheader, "\$Macro OUTLIBDIR\t\"\$SRCDIR\\lib\\XXXX\"\n" );
		}
		else
		{
			push( @fileheader, "\$Macro OUTBINDIR\t\"\$SRCDIR\\..\\game\\bin\"\n" );
		}
		push( @fileheader, "\n" );
		push( @fileheader, "\$Include \"\$SRCDIR\\vpc_scripts\\source_" );
		push( @fileheader, $configtype );
		if ( $configtype =~ /exe/ )
		{
			push( @fileheader, "_", $configurationSubsystem );
		}
		push( @fileheader, "_win32_base.vpc\"\n" );
#		push( @fileheader, "_win32_base.vpc\"\t\[\$WIN32\|\|\$LINUX\]\n" );
#		push( @fileheader, "\$Include \"\$SRCDIR\\vpc_scripts\\source_" );
#		push( @fileheader, $configtype );
#		push( @fileheader, "_x360_base.vpc\"\t\t\[\$X360\]\n" );
		push( @fileheader, "\n" );


		for ( @configurations )
		{
			my $joinedname = join( '_', $_, "Debug" );

			# print the configs
			if ( @{ $configOutput{$joinedname} } > 0 )
			{
				my($filename) = join('_', $vcprojname, $configurationNames{$_} );
				if( $outputPath )
				{
					$filename = join('/', $outputPath, $filename );
				}
				open ( OUTFILE, ">$filename.vpc" );

				print OUTFILE @fileheader;
				print OUTFILE @{ $configOutput{$joinedname} };

				$joinedname = join( '_', $_, "Release" );
				print OUTFILE @{ $configOutput{$joinedname} };

				close ( OUTFILE );
			}
		}

		$parsingFiles = 1;
	}
	elsif ( $line =~ /^\s*\<Filter$/ )
	{
		# Start of a new folder

		if ( $spaceBeforeFolder == 1 )
		{
			$spaceBeforeFolder = 0;
			for ( @configurations )
			{
				push( @{ $outputs{$_} }, "\n" );
			}
		}

		processFolder( @lines );
	}
	elsif ( $line =~ /^\s*\<File$/ )
	{
		# Start of a new file

		if ( $spaceBeforeFolder == 1 )
		{
			$spaceBeforeFolder = 0;
			for ( @configurations )
			{
				push( @{ $outputs{$_} }, "\n" );
			}
		}

		processFile( @lines );
	}
}


my $projectCt = 0;

for ( @configurations )
{
	push( @{ $outputs{$_} }, "\}\n" );

	if ( $filesAdded{$_} )
	{
		++$projectCt;

		# print the files
		my($filename) = join('_', $vcprojname, $configurationNames{$_} );
		if( $outputPath )
		{
			$filename = join('/', $outputPath, $filename );
		}
		open ( OUTFILE, ">>$filename.vpc" );
		print OUTFILE @{ $outputs{$_} };
		close ( OUTFILE );
	}
}

if ( $projectCt == 1 )
{
	my $oldname = join('_', $vcprojname, "base.vpc" );
	my $newname = join('', $vcprojname, ".vpc" );
	if( $outputPath )
	{
		$oldname = join('/', $outputPath, $oldname );
		$newname = join('/', $outputPath, $newname );
	}

	rename $oldname, $newname;
}


