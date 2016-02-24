#!/bin/bash
shopt -s expand_aliases
templateFolder="./template"
LOG_MISS_TOOL="./miss_tool.txt"
unset trace_out
unset valgrind
unset template_tar_files
colorize=false
unset keep
errCount=0
errLogFile="log_err.txt"
alias  exec=exec_func
success=0
error=1
ignore=2
alias return_on_error='err=$?; if [[ "$err" != "$success" ]]; then return $err; fi'
encodingList="raw"

print_usage() {
	echo $(basename $0)" [options] compare two folders, create and patch the first folder found and compare the result with the next folder found"
	echo "                The options are interpreted in the order in which they're entered!"
	echo " -b | --base            : base folder of versions"
	echo " -c | --colorize        : colorize output"
	echo " -e | --encoding        : (list of encoding algorithms) create patches with encoding algorithms given (could be 'raw'/'gzip'/'xz'/'zeros')"
	echo " -h | --help            : display this help and exit"
	echo " -i | --ignore-identical: (y | n) compare file of old and new version before creating/applying/checking a patch (if file are same it's ignored, default is 'yes')"
	echo " -k | --keep            : keep template files and result"
	echo " -o | --output          : output trace. Verbosely list executed command (for terminal, use /dev/stderr instead of /dev/stdout=the standard command result)"
	echo " -t | --tar             : tar name of template files"
	echo " -v | --valgrind        : execute valgrind for each command"
	exit 1
}

parse_options() {
	options=$(getopt -s bash -o b:ce:hi:ko:t:v -l base:,colorize,encoding:,help,--ignore-identical:,keep,output:,tar:,valgrind -- "$@")
	if [ "$?" != "0" ]; then
		print_usage
		exit
	fi
	eval set -- $options
	while [ $# -gt 0 ]
	do
	    case $1 in
	    -b|--base)
	    	folderBase=$(echo $2 | sed "s/'//g")
	    	shift
	    ;;
	    -c|--colorize)
	    	colorize=true
	    ;;
	    -e|--encoding)
			encodingList=$(to_lower "$2")
	    	shift
	    ;;
	    -h|--help)
	    	print_usage
	    ;;
	    -i|--ignore-identical)
	    	compare=$(to_lower $2)
	    	shift
	    ;;
	    -k|--keep)
	    	keep=true
	    ;;
	    -o|--output)
	    	trace_out=$(echo $2 | sed "s/'//g")
	    	alias exec=exec_func_out
	    	shift
	    ;;
	    -t|--tar)
	    	template_tar_files=$(echo $2 | sed "s/'//g")
	    	folderBase=$templateFolder
			encodingList="raw gzip xz zeros any"
	    	shift
	    ;;
	    -v|--valgrind)
	    	valgrind="valgrind --trace-children=yes --track-fds=yes -q "
	    ;;
		--)
			shift; break
		;;
	    -*)
	    	echo "$0: error - unrecognized option $1" 1>&2; exit 1
	    ;;
	    (*)
	    	break
	    ;;
	    esac
	    shift
	done
}

check_tool() {
	local ERR=0
	CMD="which $@"
	TOOL=$@
	HIDE_OUT=$(eval $CMD)
	ERR=$?
	if [ $ERR != "0" ]; then
	  echo $TOOL >> $LOG_MISS_TOOL
	  ERR=$error
	fi
	return $ERR
}

check_dependencies() {
	if [ -e $LOG_MISS_TOOL ]; then
		rm $LOG_MISS_TOOL
	fi
	local ERR=0
	check_tool "bsdiff"; ERR=$(($ERR + $?))
	check_tool "bspatch"; ERR=$(($ERR + $?))
	check_tool "cmp"; ERR=$(($ERR + $?))

	if [ $ERR != "0" ];then
		log_err "check_tool" "$ERR" 'Missing command '$(cat $LOG_MISS_TOOL)
		return $ERR
	fi

	return $success
}


to_lower() {
	echo $1 | tr '[:upper:]' '[:lower:]'
}


log_user() {
	str=$@
	# do nothing it it's just a change of font attribute
	if [ "${str:0:2}" = "\e" ] && [ ${#str} -le 5 ]; then
		echo -en $str>/dev/stderr
		return
	fi

	if [ -z "$trace_out" ]; then
		echo -e $str>/dev/stderr
	else
		trace $str
	fi
}


log_debug() {
	log_user $@
}

log_err() {
	cmd=$1
	err=$2
	out=$3
	msg="\n$cmd return $fontRed$err$attrReset"
	if [ "$out" != "" ]; then
		msg="$msg and message \n$fontRed$out$attrReset\n"
	fi
	msg="$msg"
	log_user -en $fontRed"\nERROR:$attrReset $msg"
	echo -e "$msg" >>$errLogFile
}

trace() {
	str=$@
	if [ "$trace_out" != "" ]; then
		if [ "${str:0:2}" = "\e" ] && [ ${#str} -le 5 ]; then
			return
		fi
		echo -e $str>>$trace_out
	fi
}

exec_func_out() {
	cmd=$@
	trace [ $fontCyan$cmd$attrReset ]
	out=$(eval $cmd)
	err=$?
	if [ "$out" != "" ]; then # avoid empty echo
		echo $out
	fi
	return $err
}

exec_func() {
	res=$(eval $@)
	err=$?
	if [ "$res" != "" ]; then
		echo $res
	fi
	return $err
}


# Initialize main folder path. Get $1(=folder_BASE = path of the initial version) and determine :
# folderNextVers = path of the next version
# folderResult = path of the folder where the patch will be stored
set_folder_base() {
	folderList=( $(exec "ls $1") )
	for folder in ${folderList[@]}
	do
		folder="$1/$folder"
		if [ -d "$folder" ]; then
			if [ -z $folderCurrVers ]; then
				folderCurrVers=$folder
			else
				folderNextVers=$folder
				break
			fi
		fi
	done
	folderResult="$folderNextVers""_PATCH"
	relativeFolder=""
	totalFiles=$(exec "ls -lR $folderCurrVers/")
	totalFiles=$(exec "ls -lR $folderCurrVers/ | grep ^- | wc -l")
	totalLink=$(exec "ls -lR $folderCurrVers/ | grep ^l | wc -l")
	totalFileCount=$(($totalFiles + $totalLink ))
}

# Increment statistic counter
inc_stat() {
	local idx=$1
	local algo=$2

	if [ "$algo" = "" ]; then
		algo="filter"
	fi

	eval stat=\$'stats_'$algo'_'$idx
	eval stats'_'$algo'_'$idx=$(($stat + 1))
}


# Before creating,applying,checking a patch, this function check than :
# this is effectively a file
# with a size in the old and previous version > 0 byte
# and extended attributes are the same
# because these cases are not managed by bsdiff
filter_file() {
	local file_src=$1
	local file_cmp=$2
	local size_src
	local size_cmp
	local err=$ignore

	fileCount=$(($fileCount + 1))
	percent=$((($fileCount * 100) / $totalFileCount))
	if [ "$progress" != "$percent" ]; then
		progress=$percent
		log_user -n $fontCyan".$progress%"$attrReset
	fi

	# not a file or not a link on file => ignore
	[[ ! -f "$file_src" && ! -L "$file_src" ]] && return $ignore

	# not found in new version  => ignore
	[ ! -e "$file_cmp" ] && inc_stat idx_FileNotFound && inc_stat idx_FileIgnored && return $ignore

	# extended attributes has changed ?
	xattrV1=( $(ls -Z $file_src) )
	xattrV2=( $(ls -Z $file_cmp) )
	[ "${xattrV1[0]}" != "${xattrV2[0]}" ] && inc_stat idx_xattrChanged && inc_stat idx_FileIgnored && return $ignore

	# File differe => analyze
	size_src=$(exec "stat -c %s $file_src 2>/dev/null")
	size_cmp=$(exec "stat -c %s $file_cmp 2>/dev/null")
	[ "$size_src" != "$size_cmp" ] && inc_stat idx_FileAnalyzed && return $success

	# File size = 0 (in old and newest version) => ignore
	[[ "$size_src" = "0" && "$size_cmp" = "0" ]] && inc_stat idx_FileEmpty && inc_stat idx_FileIgnored && return $ignore

	# Files unchanged  => ignore
	if [ "$compare" != "n" ]; then
		CHANGE=$(eval "cmp -s $file_src $file_cmp")
		[ "$?" = "0" ] && inc_stat idx_FileIgnored && inc_stat idx_FileUnchanged && return $ignore
	fi

	#  => analyze
	inc_stat idx_FileAnalyzed
	return $success

}

parse_folder() {
	local folderSrc="$folderCurrVers/$relativeFolder"
	local folderCmp="$folderNextVers/$relativeFolder"
	local folderDest="$folderResult/$relativeFolder"
	local err
	local relativeFolderStore

	cmd="ls $folderSrc"
	local file
	fileList=$(exec "$cmd")
	if [ "$?" != "0" ]; then
		log_err "$cmd" "$?" "$fileList"
		exit
	fi
	for file in $fileList
	do
		#if it is a folder (and not a link on folder), create the folder, and recurse into
		if [[ -d "$folderSrc$file" && ! -L "$folderSrc$file" ]]; then
			relativeFolderStore=$relativeFolder
			relativeFolder="$relativeFolder$file/"
			if [ ! -e "$folderDest$file" ]; then
				exec "mkdir $folderDest$file"
			fi
			parse_folder
			relativeFolder=$relativeFolderStore
			err=2
		else
			filter_file "$folderSrc$file" "$folderCmp$file"
			err=$?
			if [ "$err" = "$success" ]; then
				test_patch $file
				err=$?
			fi
		fi
	done
}

create_patch() {
	local err
	local file=$1
	local algo=$2
	local debug=$3
	fileSrc=$folderSrc$file
	fileCmp=$folderCmp$file
	filePatch="$folderDest$filename".$algo.patch
	cmd="$valgrind $diffTool $fileSrc $fileCmp $filePatch $algo $debug"
	out=$(exec "$cmd")
	err=$?
	if [ "$err" -lt "0" ]; then
		log_err "$cmd" "$err" "$out"
		inc_stat idx_ErrCreatePatch $algo
	fi
	return $err

}

apply_patch() {
	local err
	local file=$1
	local algo=$2
	oldFile=$folderSrc$file
	newFile="$folderDest$filename".$algo.patched
	patchFile="$folderDest$filename".$algo.patch
	cmd="$valgrind $patchTool $oldFile $newFile $patchFile"
	out=$(exec "$cmd 2>&1")
	err=$?
	if [ "$err" != "$success" ]; then
		log_err "$cmd" "$err" "$out"
		inc_stat idx_ErrApplyPatch $algo
	fi
	return $err
}

check_result() {
	local err
	local file=$1
	local algo=$2
	patchedFile="$folderDest$filename".$algo.patched
	newFile="$folderCmp$file"
	cmd="$checkTool $patchedFile $newFile"
	out=$(exec "$cmd 2>&1")
	err=$?
	if [ "$err" != "$success" ]; then
		log_err "$cmd" "$err" "$out \n patch not well applied on '$folderDest$filename' witch patch '$folderDest$filename.$algo.patch'"
		inc_stat idx_ErrCheckPatch $algo
	fi
	return $err
}

test_patch() {
	local file="$1"
	filename=$(basename $file)
	local err
	local algo
	if [ ! -z "$trace_out" ]; then
		debug='debug'
	fi
	for algo in ${algoList[*]}
	do
		create_patch $file $algo $debug
		[ "$?" = "0" ] && apply_patch $file $algo $debug
		[ "$?" = "0" ] && check_result $file $algo $debug
	done
	return $?
}


def_font_attributes() {
	fontCyan="\e[96m"
	fontRed="\e[91m"
	fontGreen="\e[92m"
	fontBlue="\e[34m"
	fontYellow="\e[93m"
	fontWhite="\e[97m"
	attrReset="\e[0m"
}


rm_logfile() {
	if [[ "$trace_out" != "" && -e "$trace_out" && -f "$trace_out" ]]; then
		exec "sudo rm $trace_out"
	fi
	if [ -e "$errLogFile" ]; then
		exec "sudo rm $errLogFile"
	fi
}

rm_template() {
	if [ "$keep" != "true" ]; then
		if [[ "$template_tar_files" != "" && -d "$templateFolder" ]]; then
			exec "sudo rm -rf $templateFolder"
		fi
	fi
}

rm_result() {
	if [ "$keep" != "true" ]; then
		if [ -d "$folderResult" ]; then
			exec "rm -rf $folderResult"
		fi
	fi
}

test() {
	parse_folder "$folderCurrVers"
}

init() {
	# Check than all variables are declared
	# set -o nounset
	compare='y'
	parse_options "$@"

	diffTool=$(which bsdiff)
	patchTool=$(which bspatch)
	checkTool=$(which cmp)
	checkTool="$checkTool -s "

	if [[ -z "$template_tar_files" && -z "$folderBase" ]]; then
		print_usage
	fi
	if [ "$colorize" = "true" ]; then
		def_font_attributes
	fi

	check_dependencies
	if [ "$?" != "$success" ]; then
		exit $?
	fi
	rm_logfile
	rm_template

	if [ "$encodingList" != "" ]; then
		algoList=$encodingList
	else
		algoList="raw"
	fi


	if [ ! -e "$templateFolder" ]; then
		exec "mkdir $templateFolder"
	fi
	if [ "$template_tar_files" != "" ]; then
		exec "sudo tar --preserve-permissions --xattrs --selinux -p -xf $template_tar_files -C $templateFolder/"
	fi
	set_folder_base $folderBase
	log_user "folder of current version (folderCurrVers)=$fontGreen$folderCurrVers$attrReset"
	log_user "folder of next (newest) version (folderNextVers)=$fontGreen$folderNextVers$attrReset"
	log_user "folder of patch (and patched) files (folderResult)=$fontGreen$folderResult$attrReset"
	log_user "Found $fontGreen$totalFileCount$attrReset file/link"
	log_user "diffTool used=$fontGreen$diffTool$attrReset"
	log_user "patchTool used=$fontGreen$patchTool$attrReset"
	log_user "checkTool used=$fontGreen$checkTool$attrReset"

	if [ -d "$folderResult" ]; then
		read -e -i "Y" -p "A result folder have been found ($folderResult). Do you want to remove this folder? ('Y'es/'n'o/'q'uit): "
		case $REPLY in
			n | N)
				# do nothing
			;;
			y | Y)
				exec "rm -rf $folderResult"
			;;
			q | Q)
				exit 0
		esac
	fi
	if [ ! -e "$folderResult" ]; then
		exec "mkdir $folderResult"
	fi

	stats=(0 0 0 0 0 0 0 0)

	idx_ErrCreatePatch=0
	idx_ErrApplyPatch=1
	idx_ErrCheckPatch=2
	idx_FileAnalyzed=3
	idx_FileIgnored=4
	idx_FileNotFound=5
	idx_FileUnchanged=6
	idx_FileEmpty=7
	idx_xattrChanged=8
}

show_stat() {
	duration=$(($endTime - $startTime))
	log_user "\n-------------------------------------"
	log_user "script start at $(date +%c --date @$startTime), ended at  $(date +%c --date @$endTime)\n"
	log_user "Time to process=$fontGreen$(date +%Mm:%Ss --date @$duration) \n$attrReset"
	log_user "-------------------------------------"
	[ "$stats_filter_idx_FileAnalyzed" = "" ] && stats_filter_idx_FileAnalyzed=0
	log_user $fontBlue"Analysed files               : $fontGreen $stats_filter_idx_FileAnalyzed"$attrReset

	[ "$stats_filter_idx_FileIgnored" = "" ] && stats_filter_idx_FileIgnored=0
	log_user $fontBlue"Ignored files                : $stats_filter_idx_FileIgnored"$attrReset

	[ "$stats_filter_idx_FileEmpty" = "" ] && stats_filter_idx_FileEmpty=0
	log_user $fontBlue".Empty files                 : $stats_filter_idx_FileEmpty"$attrReset
	[ "$stats_filter_idx_FileNotFound" = "" ] && stats_filter_idx_FileNotFound=0
	log_user $fontBlue".Not Found in new version    : $stats_filter_idx_FileNotFound"$attrReset
	[ "$stats_filter_idx_xattrChanged" = "" ] && stats_filter_idx_xattrChanged=0
	log_user $fontBlue".Extended attributes have changed    : $stats_filter_idx_xattrChanged"$attrReset
	if [ "$compare" != "n" ]; then
		[ "$stats_filter_idx_FileUnchanged" = "" ] && stats_filter_idx_FileUnchanged=0
		log_user $fontBlue".Unchanged files              : $stats_filter_idx_FileUnchanged"$attrReset
	fi
	errCount=0
	for algo in ${algoList[*]}
	do
		eval stat_CreatePatch=\$'stats_'$algo'_idx_ErrCreatePatch'; [ "$stat_CreatePatch" = "" ] && stat_CreatePatch=0
		eval stat_ApplyPatch=\$'stats_'$algo'_idx_ErrApplyPatch'; [ "$stat_ApplyPatch" = "" ] && stat_ApplyPatch=0
		eval stat_CheckPatch=\$'stats_'$algo'_idx_ErrCheckPatch'; [ "$stat_CheckPatch" = "" ] && stat_CheckPatch=0

		eval errCount=$(( $stat_CreatePatch + $stat_ApplyPatch + $stat_CheckPatch ))
		if [ "$errCount" = 0 ] ; then
			log_user $fontGreen"With '$algo' encoding, count $errCount errors."
		else
			log_user $fontRed"With '$algo' encoding, count $errCount errors, which is :"

			eval stat=\$'stats_'$algo'_idx_ErrCreatePatch'; [ "$stat" = "" ] && stat=0
			log_user $fontBlue".on patch created : $fontRed $stat"$attrReset

			eval stat=\$'stats_'$algo'_idx_ErrApplyPatch'; [ "$stat" = "" ] && stat=0
			log_user $fontBlue".on patch applied : $fontRed $stat"$attrReset

			eval stat=\$'stats_'$algo'_idx_ErrCheckPatch'; [ "$stat" = "" ] && stat=0
			log_user $fontBlue".on patch checked : $fontRed $stat"$attrReset
			fi
	done
	if [ "$errCount" -gt "0" ]; then
		log_user $fontRed"see '$errLogFile' for more informations"
		log_user $attrReset
	fi
}

cleanup() {
	rm_template
	rm_result
}

main() {
	PATH_STORE=$PATH
	PATH=.:..:../..:$PATH
	startTime=$(date +%s)
	init "$@"
	parse_folder "$folderCurrVers"
	endTime=$(date +%s)
	show_stat
	cleanup
	PATH=$PATH_STORE
}

main "$@"

