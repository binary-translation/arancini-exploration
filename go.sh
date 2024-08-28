#! /bin/bash

export ARANCINI_ENABLE_LOG=false

try_ssh() {
	local host=$1;
	local cmd=$2;

	ssh -o ConnectTimeout=5 $host $cmd;
	return $?;
}

get_phoenix() {
	local arch=$1;
	local cmd = "nix build ./scripts\#phoenix.x86_64-linux --out-link phoenix-x86_64";

	if [[ $arch != "x86_64" ]]; then
		echo "Cross building the phoenix benchmarks?"
		local ret=$(try_ssh $URL_X86_ARANCINI $cmd);
		if [[ $ret != 0 ]]; then
			echo "Failed to build the phoenix benchmarks on remote x86";
		fi;
		if [[ ret == 0 ]]; then

			nix copy --from ssh://$URL_X86_ARANCINI ./phoenix-x86_64;
		fi;
		 
		cross:
		if [[ $FORCE_CROSS_BENCH == false ]]; then
			echo "Use -f to force cross building";
			exit 1;
		fi;
	fi;

	nix build ./scripts\#phoenix.x86_64-linux --out-link phoenix-x86_64;
	PHOENIX_DIR=phoenix-x86_64;
}
get_parsec() {
	local arch=$1;

	if [[ $arch != "x86_64" ]]; then
		echo "Cross building the parsec benchmarks? consider using >nix copy< instead"
		if [[ $FORCE_CROSS_BENCH == false ]]; then
			echo "Use -f to force cross building";
			exit 1;
		fi;
	fi;

	nix build ./scripts\#parsec.x86_64-linux --out-link parsec-x86_64;
	PARSEC_DIR=parsec-x86_64;
}

check() {
	if [[ ! -x $1/lib/libarancini-runtime.so ]]; then
		echo "Error: No runtime lib in $1";
		exit 1;
	fi;
}

check() {
	if [[ ! -x $1/bin/txlat ]]; then
		echo "Error: No txlat in $1";
		exit 1;
	fi;
}

run_txlat_lib() {
	local result_dir=$1;
	local bench_dir=$2;
	local out_dir=$3;
	local lib=$4
	local ver=$5;
	local extra=$6;

	local args="";
	if [[ $ver == "-dyn" ]]; then
		$args="--no-static";
	fi;
	if [[ $ver == "-nmem" ]]; then
		$args="--nlib general.mni";
	fi;

	echo "Running <<< ./$result_dir/bin/txlat -I $bench_dir/$lib.so -O $txlat_dir/$lib$ver.out $args $extra >>>"

	./$result_dir/bin/txlat -I $bench_dir/$lib.so -O $txlat_dir/$lib$ver.out $args $extra &> $out_dir/$lib$ver-dump.txt;
}

run_txlat() {
	local result_dir=$1;
	local bench_dir=$2;
	local out_dir=$3;
	local file=$4
	local ver=$5;
	local extra=$6;

	local args="";
	if [[ $ver == "-dyn" ]]; then
		$args="--no-static";
	fi;
	if [[ $ver == "-nmem" ]]; then
		$args="--nlib general.mni";
	fi;

	echo "Running <<< ./$result_dir/bin/txlat -I $bench_dir/$file -O $txlat_dir/$file$ver.out $args $extra >>>"

	./$result_dir/bin/txlat -I $bench_dir/$file -O $txlat_dir/$file$ver.out $args $extra &> $out_dir/$file$ver-dump.txt;
}

do_tx() {
	local arch=$1;
	local tx_lib=0;
	local result_dir=result-$arch;
	local txlat_dir=txlat-$arch;

	check ./$result_dir;

	mkdir -p $txlat_dir;
	if [[ -f $txlat_dir/commit.txt ]]; then
		if [[ $(git rev-parse HEAD) == $(cat $txlat_dir/commit.txt) ]]; then
			echo "Already up to date";
			tx_lib=1
		fi;
	fi;

	if [[ $tx_lib == 0 ]]; then
		./$result_dir/bin/txlat -I ${PHOENIX_DIR}/libc.so -O $txlat_dir/libmusl.out &> $txlat_dir/libmusl-dump.txt;
		./$result_dir/bin/txlat -I ${PHOENIX_DIR}/libc.so -O $txlat_dir/libmusl-nmem.out --nlib general.mni &> $txlat_dir/libmusl-nmem-dump.txt;
		
		
		#./$result_dir/bin/txlat -I ${PARSEC_DIR}/libc.so -O $txlat_dir/libmusl.out &> $txlat_dir/libmusl-dump.txt;
		./$result_dir/bin/txlat -I ${PARSEC_DIR}/libc++.so -O $txlat_dir/libc++.out --nlib general.mni &> $txlat_dir/libmusl-nmem-dump.txt;
	fi;
	git rev-parse HEAD > $txlat_dir/commit.txt;

	for p in $(ls ${PHOENIX_DIR});
	do
		if [[ ! -x ${PHOENIX_DIR}/$p ]]; then continue; fi;
		if [[ $p == "libc.so" ]]; then continue; fi;
		#TODO: no datafiles
		echo $p;
		./$result_dir/bin/txlat -I ${PHOENIX_DIR}/$p -O $txlat_dir/$p.out -l $txlat_dir/libmusl.out &> $txlat_dir/$p-dump.txt;
		#
		#./$result_dir/bin/txlat -I ${PHOENIX_DIR}/$p -O $txlat_dir/$p-dyn.out --no-static -l $txlat_dir/libmusl-dyn.out &> $txlat_dir/$p-dyn-dump.txt;
		./$result_dir/bin/txlat -I ${PHOENIX_DIR}/$p -O $txlat_dir/$p-nmem.out -l $txlat_dir/libmusl-nmem.out &> $txlat_dir/$p-nmem-dump.txt;
	done
	for p in $(ls ${PARSEC_DIR});
	do
		if [[ ! -x ${PARSEC_DIR}/$p ]]; then continue; fi;
		if [[ $p == "libc.so" ]]; then continue; fi;
		#TODO: no datafiles
		echo $p;
		./$result_dir/bin/txlat -I ${PARSEC_DIR}/$p -O $txlat_dir/$p.out -l $txlat_dir/libmusl.out &> $txlat_dir/$p-dump.txt;
		#./$result_dir/bin/txlat -I ${PARSEC_DIR}/$p -O $txlat_dir/$p-dyn.out --no-static -l $txlat_dir/libmusl-dyn.out &> $txlat_dir/$p-dyn-dump.txt;
		./$result_dir/bin/txlat -I ${PARSEC_DIR}/$p -O $txlat_dir/$p-nmem.out -l $txlat_dir/libmusl-nmem.out &> $txlat_dir/$p-nmem-dump.txt;
	done
}

translate_all() {
	local arch=$1;
	local system=$(uname -m);

	if [[ $arch == "" ]]; then arch=$system; fi;

	get_phoenix $system;
	get_parsec $system;

	if [[ $system != $arch ]]; then
		do_cross $arch;
	else
		do_tx $arch;
	fi;
}

print_help() {
	echo "Usage: $0 [-a <arch>] [-h] [-f] [-r <path>]";
	echo "  -a <arch>: Translate all benchmarks for the given architecture (default: uname -m)";
	echo "  -h: Print this help message";
	echo "  -f: Force cross building the benchmarks";
	echo "  -r <path>: Path to the Arancini runtime library for <arch> (only used when cross translating)";
	echo "  -u <url>: URL to the remote Arancini repository for <arch> (only used when cross translating)";
	echo "  -x <url>: URL to the x86 Arancini repository (only used to build the benchmarks on non-x86 systems)";
}

main () {
	local arch="";

	while getopts "a:hfr:u:" opt; do
		case $opt in
			a)
				$arch=$OPTARG;
				exit 0;
				;;
			h)
				print_help;
				exit 0;
				;;
			f)
				FORCE_CROSS_BENCH=true;
				;;
			r)
				CROSS_RUNTIME_PATH=$OPTARG;
				;;
			u)
				URL_REMOTE_ARANCINI=$OPTARG;
				;;
			x)
				URL_X86_ARANCINI=$OPTARG;
				;;
			\?)
				echo "Invalid option: -$OPTARG" >&2;
				print_help;
				exit 1;
				;;
		esac;
	done;

	if [[ $URL_X86_ARANCINI != "" && $URL_REMOTE_ARANCINI != "" ]]; then
		echo "Cross compiling on non-x86? Bold move. Or a mistake.";
	fi;
	
	translate_all $arch;
}

main;
