{ writeShellScriptBin
, getopt
, xorg
, hl2-unwrapped
}:

writeShellScriptBin "hl2-wrapper" ''
  config_path=$HOME/.config/hl2/config
  default_resource_path=$HOME/hl2

  longoptions="resource-path:,impermanent,help"
  shortoptions="r:h"

  impermanent=false

  usage() {
    echo "hl2(-wrapper) usage:"
    echo "Define resource_path variable in $config_path, with the argument shown below or put resource files in $default_resource_path."
    grep ' \+-.*) ' $0 | sed 's/#//' | sed -r 's/([a-z])\)/\1/'
    exit 0
  }

  # Just in case
  save_newly_created_files() {
    newly_created_files=$(cd $tmp_dir && find . -type f | cut -c 2-)

    echo "$newly_created_files" | while read file;
    do
      cp $tmp_dir$file $resource_path$file
    done
  }

  cleanup() {
    if ! $impermanent; then
      save_newly_created_files
    fi
    
    rm -rf $tmp_dir
  }

  if [[ -e $config_path ]]
  then
    source $config_path
  fi

  parsed=$(${getopt}/bin/getopt -l $longoptions -o $shortoptions -a -- "$@")

  eval set -- "$parsed"

  while true; do
    case "$1" in
      -r | --resource-path) 	# Defines resources folder path
        resource_path="$2"
	shift 2
        ;;
      -p | --parameters)	# Sets parameters passed to hl2 launcher (encapsulated with quotes)
        launcher_parameters="$2"
        shift 2
        ;;
      --impermanent) 		# Don't save newly created files that are not in symlimked folders (you probably don't want to use this)
        impermanent=true
	shift 1
	;;
      -h | --help) 		# Get help
	usage
        exit 0
        ;;
      --)
	shift
	break
	;;
      *)
        echo "Wrong argument: $1"
	usage
	exit 1
	;;
    esac
  done

  if [[ ! $resource_path ]]
  then
    echo "The path to Half-Life 2 resource folder is not set either in a config file or with an argument. Looking for resource files in the standard directory (~/hl2)."
    resource_path=$default_resource_path
  fi

  if [[ ! -d $resource_path ]]
  then
    echo "$resource_path doesn't exist. Set a proper resource path."
    exit 1
  elif [[ ! -d $resource_path/hl2 || ! -d $resource_path/platform ]]
  then
    echo "$resource_path doesn't contain 'hl2' and/or 'platform' folder. Set a proper resource path."
    exit 1
  fi

  tmp_dir=$(mktemp -d)
  echo $tmp_dir

  mkdir $tmp_dir/{hl2,platform}
  ln -s $resource_path/hl2/* $tmp_dir/hl2/
  ln -s $resource_path/platform/* $tmp_dir/platform/
  rm -rf $tmp_dir/hl2/bin

  ${xorg.lndir}/bin/lndir "${hl2-unwrapped}" $tmp_dir

  trap cleanup EXIT

  ( cd $tmp_dir ; ./hl2_launcher $launcher_parameters )
''
