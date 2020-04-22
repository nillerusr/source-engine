#!/bin/sh
#
#       Copyright (c) 2004, Valve LLC. All rights reserved.
#
#	a wrapper script for the main Source engine dedicated server binary.
#	Performs auto-restarting of the server on crash. You can
#	extend this to log crashes and more.
#

# setup the libraries, local dir first!
export LD_LIBRARY_PATH=".:bin:$LD_LIBRARY_PATH"

init() {
	# Initialises the various variables
	# Set up the defaults
	GAME=""
	DEBUG=""
	RESTART="yes"
	HL=./srcds_i486
	HL_LOCATION=./
	HL_DETECT=1
	TIMEOUT=10 # time to wait after a crash (in seconds)
	CRASH_DEBUG_MSG="email debug.log to linux@valvesoftware.com"
	GDB="gdb" # the gdb binary to run
	DEBUG_LOG="debug.log"
	PID_FILE="" # only needed it DEBUG is set so init later
	STEAM=""
	PID_FILE_SET=0
	STEAMERR=""
	SIGINT_ACTION="quit 0" # exit normally on sig int
	NO_TRAP=0
	AUTO_UPDATE=""
	STEAM_USER=""
	STEAM_PASSWORD=""
	PARAMS=$*
	SCRIPT_LOCATION=$PWD

	# Remove any old default pid files
	# Cant do this as they may be still running
	#rm -f hlds.*.pid

	# use the $FORCE environment variable if its set
	if test -n "$FORCE" ; then
		# Note: command line -binary will override this
		HL=$FORCE
		HL_DETECT=0
	fi

	while test $# -gt 0; do
		case "$1" in
		"-game")
			GAME="$2"
			shift ;;
		"-debug")
			DEBUG=1
			# Ensure that PID_FILE is set
			PID_FILE_SET=1
			if test -z "$PID_FILE"; then
				PID_FILE="hlds.$$.pid"
			fi ;;
		"-norestart")
			RESTART="" ;;
		"-pidfile")
			PID_FILE="$2"
			PID_FILE_SET=1
			shift ;;
		"-binary")
			HL="$2"
			HL_DETECT=0
			shift ;;
		"-timeout")
			TIMEOUT="$2"
			shift ;;
		"-gdb")
			GDB="$2"
			shift ;;
		"-debuglog")
			DEBUG_LOG="$2"
			shift ;;
		"-autoupdate")
			AUTO_UPDATE="yes"
			STEAM="./steam"
			RESTART="yes" ;;
		"-steamerr")
			STEAMERR=1 ;;
		"-ignoresigint")
			SIGINT_ACTION="" ;;
		"-notrap")
			NO_TRAP=1 ;;
		"-steamuser")
			STEAM_USER="$2";
			shift ;;
		"-steampass")
			STEAM_PASSWORD="$2";
			shift ;;
		"-help")
			# quit with syntax
			quit 2
			;;
		esac
		shift
	done

        # Ensure we have a game specified - if none is then default to CSS
        if test -z "$GAME"; then
                GAME="cstrike"
                PARAMS="$PARAMS -game $GAME"
        fi

        # If the game exists in the 'orangebox' directory then run that version instead
        # of the one in ./
        if test -f "orangebox/$GAME/gameinfo.txt"; then
                HL_LOCATION=./orangebox/
        else
                HL_LOCATION=./
        fi

	# Ensure that the game directory exists at all
	if test ! -d "$HL_LOCATION$GAME"; then
		echo "Invalid game type '$GAME' sepecified."
		quit 1
	fi

	if test 0 -eq "$NO_TRAP"; then
		# Set up the int handler
		# N.B. Dont use SIGINT symbolic value
		#  as its just INT under ksh
		trap "$SIGINT_ACTION" 2
	fi

	# Only detect the CPU if it hasnt been set with
	# either environment or command line
	if test "$HL_DETECT" -eq 1; then
		detectcpu
	fi

	if test ! -f "$HL_LOCATION$HL"; then
		echo "Source Engine binary '$HL_LOCATION$HL' not found, exiting"
		quit 1
	elif test ! -x "$HL_LOCATION$HL"; then
		# Could try chmod but dont know what we will be
		# chmoding so just fail.
		echo "Source engine binary '$HL_LOCATION$HL' not executable, exiting"
		quit 1
	fi

	# Setup debugging
	if test -n "$DEBUG" ; then
		#turn on core dumps :) (if possible)
		echo "Enabling debug mode"
		if test "unlimited" != `ulimit -c` && test "`ulimit -c`" -eq 0 ; then
			ulimit -c 2000
		fi
		GDB_TEST=`$GDB -v`
		if test -z "$GDB_TEST"; then
			echo "Please install gdb first."
			echo "goto http://www.gnu.org/software/gdb/ "
			DEBUG="" # turn off debugging cause gdb isn't installed
		fi
	fi

	if test -n "$STEAM_PASSWORD" && test -z "$STEAM_USER"; then
		echo "You must set both the steam username and password."
		quit 1
	fi

	#if test 1 -eq $PID_FILE_SET && test -n "$PID_FILE"; then
	#	HL_CMD="$HL $PARAMS -pidfile $PID_FILE"
	#else
		HL_CMD="$HL $PARAMS"
	#fi
}

syntax () {
	# Prints script syntax

	echo "Syntax:"
	echo "$0 [-game <game>] [-debug] [-norestart] [-pidfile]"
	echo "	[-binary [srcds_i486]"
	echo "	[-timeout <number>] [-gdb <gdb>] [-autoupdate]"
	echo "	[-steamerr] [-ignoresigint] [-steamuser <username>]"
	echo "	[-steampass <password>] [-debuglog <logname>]"
	echo "Params:"
	echo "-game <game>        	Specifies the <game> to run."
	echo "-debug              	Run debugging on failed servers if possible."
	echo "-debuglog <logname>	Log debug output to this file."
	echo "-norestart          	Don't attempt to restart failed servers."
	echo "-pidfile <pidfile>  	Use the specified <pidfile> to store the server pid."
	echo "-binary <binary>    	Use the specified binary ( no auto detection )."
	echo "-timeout <number>   	Sleep for <number> seconds before restarting"
	echo "			a failed server."
	echo "-gdb <gdb>          	Use <dbg> as the debugger of failed servers."
	echo "-steamerr     	  	Quit on steam update failure."
	echo "-steamuser <username>	Use this username for steam updates."  
	echo "-steampass <password>	Use this password for steam updates" 
	echo "			(-steamuser must be specified as well)."
	echo "-ignoresigint       	Ignore signal INT ( prevents CTRL+C quitting"
	echo "			the script )."
	echo "-notrap             	Don't use trap. This prevents automatic"
	echo "			removal of old lock files."
	echo ""
	echo "Note: All parameters specified as passed through to the server"
	echo "including any not listed."
}

debugcore () {
	# Debugs any core file if DEBUG is set and
	# the exitcode is none 0

	exitcode=$1

	if test $exitcode -ne 0; then
		if test -n "$DEBUG" ; then 
			echo "bt" > debug.cmds;
			echo "info locals" >> debug.cmds;
			echo "info sharedlibrary" >> debug.cmds
			echo "info frame" >> debug.cmds;  # works, but gives an error... must be last
			echo "----------------------------------------------" >> $DEBUG_LOG
			echo "CRASH: `date`" >> $DEBUG_LOG
			echo "Start Line: $HL_CMD" >> $DEBUG_LOG

			# check to see if a core was dumped
			if test -f core ; then
				CORE="core"
			elif test -f core.`cat $PID_FILE`; then
				CORE=core.`cat $PID_FILE`
			elif test -f "$HL_LOCATION$HL.core" ; then
				CORE="$HL_LOCATION$HL.core"
			fi
			
			if test -n "$CORE"; then
				$GDB $HL_LOCATION$HL $CORE -x debug.cmds -batch >> $DEBUG_LOG
			fi
		
			echo "End of Source crash report" >> $DEBUG_LOG
			echo "----------------------------------------------" >> $DEBUG_LOG
			echo $CRASH_DEBUG_MSG
			rm debug.cmds
		else
			echo "Add \"-debug\" to the $0 command line to generate a debug.log to help with solving this problem"
		fi
	fi
}

detectcpu() {
	# Attempts to auto detect the CPU
	echo "Auto detecting CPU"

	if test -e /proc/cpuinfo; then
		CPU_VERSION="`grep "cpu family" /proc/cpuinfo | cut -f2 -d":" | tr -d " " | uniq`";
		if test $CPU_VERSION -lt 4; then
			echo "Error: srcds REQUIRES a 486 CPU or better";
			quit 1
		elif test $CPU_VERSION -ge 6; then
			FEATURES="`grep 'flags' /proc/cpuinfo`";
			SSE2="`echo $FEATURES |grep -i SSE2`"
			AMD="`grep AMD /proc/cpuinfo`";
			if test -n "$AMD"; then
				OPTERON="`grep Opteron /proc/cpuinfo`";
				PLATFORM="`uname -m`"
				if test -z "$OPTERON"; then
					OPTERON="`grep "Athlon HX" /proc/cpuinfo`";
					if test -z "$OPTERON"; then
						OPTERON="`grep "Athlon(tm) 64" /proc/cpuinfo`";
					fi
				fi

				if test -n "$OPTERON" && test "x86_64" = "$PLATFORM"; then
					echo "Using AMD-Opteron (64 bit) Optimised binary."
					HL=./srcds_amd
				else
					echo "Using AMD Optimised binary."
					HL=./srcds_amd
				fi
			elif  test -n "$SSE2"; then
				# CPU supports SSE2 P4 +
				echo "Using SSE2 Optimised binary."
				HL=./srcds_i686
			else
				echo "Using default binary."
			fi		
		else
			echo "Using default binary."
		fi

	elif test "FreeBSD" = `uname`; then
		CPU="`grep 'CPU:' /var/run/dmesg.boot`"
		FEATURES="`grep 'Features=' /var/run/dmesg.boot`"
		AMD="`echo $CPU |grep AMD`"
		I686="`echo $CPU |grep 686`"
		SSE2="`echo $FEATURES |grep -i SSE2`"
		if test -n "$AMD"; then
			echo "Using AMD Optimised binary."
			HL=./srcds_amd
		elif test -n "$SSE2" ; then
			echo "Using SSE2 Optimised binary."
			HL=./srcds_i686
		else
			echo "Using default binary."
		fi
	else
		echo "Using default binary."
	fi
}

update() {
	updatesingle
}

updatesingle() {
	# Run the steam update
	# exits on failure if STEAMERR is set

	if test -n "$AUTO_UPDATE"; then
		if test -f "$STEAM"; then
			echo "Updating server using Steam."
		
			if test "$GAME" = "cstrike"; then
				GAME="Counter-Strike Source";
			fi
			if test "$GAME" = "dod"; then
				GAME="dods";
			fi

			CMD="$STEAM -command update -dir ."; 
			if test -n "$STEAM_USER"; then
				CMD="$CMD -username $STEAM_USER";
			fi 
			if test -n "$STEAM_PASSWORD"; then
				CMD="$CMD -password $STEAM_PASSWORD";
			fi 

			$CMD -game "$GAME"
			if test $? -ne 0; then
				if test -n "$STEAMERR"; then
					echo "`date`: Steam Update failed, exiting."
					quit 1
				else
					echo "`date`: Steam Update failed, ignoring."
					return 0
				fi
			fi
		else
			if test -n "$STEAMERR"; then
				echo "Could not locate steam binary:$STEAM, exiting.";
				quit 1
			else
				echo "Could not locate steam binary:$STEAM, ignoring.";
				return 0
			fi
		fi
	fi

	return 1
}
	
run() {
	# Runs the steam update and server
	# Loops if RESTART is set
	# Debugs if server failure is detected
	# Note: if RESTART is not set then
	# 1. DEBUG is set then the server is NOT exec'd
	# 2. DEBUG is not set the the server is exec'd

	if test -n "$RESTART" ; then
		echo "Auto-restarting the server on crash"

		#loop forever
		while true
		do
			# Update if needed
			update

			# Run the server
			cd $HL_LOCATION
			$HL_CMD
			retval=$?
			cd $SCRIPT_LOCATION
			if test $retval -eq 0 && test -z "$AUTO_UPDATE"; then
				break; # if 0 is returned then just quit
			fi

			debugcore $retval

			echo "`date`: Server restart in $TIMEOUT seconds"

			# don't thrash the hard disk if the server dies, wait a little
			sleep $TIMEOUT
		done # while true 
	else
		# Update if needed
		update

		# Run the server
		if test -z "$DEBUG"; then
			# debug not requested we can exec
			exec $HL_CMD
		else
			# debug requested we can't exec
			$HL_CMD
			debugcore $?
		fi
	fi
}

quit() {
	# Exits with the give error code, 1
	# if none specified.
	# exit code 2 also prints syntax
	exitcode="$1"

	# default to failure
	if test -z "$exitcode"; then
		exitcode=1
	fi

	case "$exitcode" in
	0)
		echo "`date`: Server Quit" ;;
	2)
		syntax ;;
	*)
		echo "`date`: Server Failed" ;;
	esac

	# Remove pid file
	if test -n "$PID_FILE" && test -f "$PID_FILE" ; then
		# The specified pid file
		rm -f $PID_FILE
	fi

	# reset SIGINT and then kill ourselves properly
	trap - 2
	kill -2 $$
}

# Initialise
init $*

# Run
run

# Quit normally
quit 0

