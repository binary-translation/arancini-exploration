#! /bin/bash

export ARANCINI_ENABLE_LOG=false

get_phoenix() {
	local arch=$1;
	nix build ./scripts\#phoenix.x86_64-linux --out-link phoenix-x86_64;
	PHOENIX_DIR=phoenix-x86_64;
}

check() {
	if [[ ! -x $1/bin/txlat ]]; then
		echo "Error: No txlat in $1";
		exit 1;
	fi;
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
		./$result_dir/bin/txlat -I ${PHOENIX_DIR}/libc.so -O $txlat_dir/libmusl-dyn.out --no-static &> $txlat_dir/libmusl-dyn-dump.txt;
	fi;
	git rev-parse HEAD > $txlat_dir/commit.txt;

	for p in $(ls ${PHOENIX_DIR});
	do
		if [[ ! -x ${PHOENIX_DIR}/$p ]]; then continue; fi;
		if [[ $p == "libc.so" ]]; then continue; fi;
		#TODO: no datafiles
		echo $p;
		./$result_dir/bin/txlat -I ${PHOENIX_DIR}/$p -O $txlat_dir/$p.out -l $txlat_dir/libmusl.out &> $txlat_dir/$p-dump.txt;
		./$result_dir/bin/txlat -I ${PHOENIX_DIR}/$p -O $txlat_dir/$p-dyn.out --no-static -l $txlat_dir/libmusl-dyn.out &> $txlat_dir/$p-dyn-dump.txt;
	done
}

translate_all() {
	local arch=$1;
	local system=$(uname -m);

	if [[ $arch == "" ]]; then arch=$system; fi;

	get_phoenix $arch;

	if [[ $system != $arch ]]; then
		do_cross $arch;
	else
		do_tx $arch;
	fi;
}

translate_all $1;
