{
	inputs = {
		nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
		flake-utils.url = "github:numtide/flake-utils";
		phoenix-src = {
			type = "github";
			owner = "ReimersS";
			repo = "phoenix";
			ref = "musl";
			flake = false;
		};
		phoenix-16 = {
			type = "github";
			owner = "ReimersS";
			repo = "phoenix";
            rev = "66c4c3904133b559a6c8787b8cd445d32b2343f5";
			flake = false;
		};
		parsec-src = {
			type = "github";
			owner = "ReimersS";
			repo = "parsec-benchmark";
			ref = "master";
			flake = false;
		};
		risotto-pkgs = {
			type = "github";
			owner = "NixOS";
			repo = "nixpkgs";
			rev = "80b3160c21977e627ae99f0c87404cdcd85646ad";
		};
		risotto-bench = {
			type = "git";
            url = "https://github.com/taugoust/risotto-artifact-asplos23";
			ref = "main";
			flake = false;
            submodules = true;
		};
	};

	nixConfig.extra-substituters = [ "https://musl-toolchains.cachix.org" ];
	nixConfig.extra-trusted-public-keys = [ "musl-toolchains.cachix.org-1:g9L50mmWHHMzAVIfgLVQjhoBsjT66n3LDa0f8xeigpI=" ];

	outputs = { self, nixpkgs, flake-utils, phoenix-src, phoenix-16, parsec-src, risotto-pkgs,
    risotto-bench, ... }:
	flake-utils.lib.eachSystem [ "x86_64-linux" "riscv64-linux" "aarch64-linux" ] (system:
	let
		pkgs = import nixpkgs { system = system; crossSystem = { config = system+"-musl"; useLLVM = true; linker = "lld"; }; };
		native_pkgs = import nixpkgs { inherit system; };
		risotto_pkgs = import risotto-pkgs { inherit system; };

		rv_patch = builtins.fetchurl {
			url = "https://gist.githubusercontent.com/ReimersS/81e6d9b7ba90b42800be1f8d7443689c/raw/06e2ce53033b5eb568638de1171c88ad677f1777/riscv-fpargs.patch";
			sha256 = "1bywsiml8gd22yjqcngr0sdk9xixy6axx97yars19xyja2sz4dj5";
		};
        qemu = native_pkgs.qemu.override { pipewireSupport=false;
        hostCpuTargets=["x86_64-linux-user"]; jackSupport=false; alsaSupport=false;
        gtkSupport=false; vncSupport=false; pulseSupport=false; smartcardSupport=false;
        spiceSupport=false; glusterfsSupport=false; openGLSupport=false; sdlSupport=false;
        usbredirSupport=false; xenSupport=false; cephSupport=false; virglSupport=false;};
		risotto-qemu = native_pkgs.stdenv.mkDerivation {
			name = "risotto-qemu";
			src = builtins.fetchGit {
				url = "https://github.com/rgouicem/qemu.git";
				rev = "a73de0b0c1190af7d7e5e8ed73b4eb780acde2c5";
				ref = "master-6.1.0";
				submodules = true;
			};
			buildInputs = with native_pkgs; [ perl ninja meson flex bison zlib gnumake python3 pkg-config glib ];
			useNinjaBuildPhase = false;
			useNinjaInstallPhase = false;
			useMesonConfigurePhase = false;
			configurePhase = ''
				chmod +x ./scripts/shaderinclude.pl
				patchShebangs .
				./configure --target-list=x86_64-linux-user
			'';
			buildPhase = ''
				make -j$(nproc)
			'';
			installPhase = ''
				mkdir -p $out/bin
				cp build/qemu-x86_64 $out/bin/risotto-qemu
			'';
		};
		risotto = native_pkgs.stdenv.mkDerivation {
			name = "risotto";
			src = builtins.fetchGit {
				url = "https://github.com/rgouicem/qemu.git";
				rev = "1fca2ddf96cc78cf6dd9dab4edf7a354718b08de";
				ref = "risotto";
				submodules = true;
			};
			buildInputs = with native_pkgs; [ perl ninja meson flex bison zlib gnumake python3 pkg-config glib ];
			useNinjaBuildPhase = false;
			useNinjaInstallPhase = false;
			useMesonConfigurePhase = false;
			configurePhase = ''
				chmod +x ./scripts/shaderinclude.pl
				patchShebangs .
				./configure --target-list=x86_64-linux-user
			'';
			buildPhase = ''
				make -j$(nproc)
			'';
			installPhase = ''
				mkdir -p $out/bin
				cp build/qemu-x86_64 $out/bin/risotto
			'';

			patches = [ rv_patch ];
		};
		risotto-nofence = native_pkgs.stdenv.mkDerivation {
			name = "risotto-nofence";
			src = builtins.fetchGit {
				url = "https://github.com/rgouicem/qemu.git";
				rev = "f31902ed15b2b86bac4f06f7899919be1c649ea9";
				ref = "no-fences";
				submodules = true;
			};
			buildInputs = with native_pkgs; [ perl ninja meson flex bison zlib gnumake python3 pkg-config glib ];
			useNinjaBuildPhase = false;
			useNinjaInstallPhase = false;
			useMesonConfigurePhase = false;
			configurePhase = ''
				chmod +x ./scripts/shaderinclude.pl
				patchShebangs .
				./configure --target-list=x86_64-linux-user
				'';
			buildPhase = ''
				make -j$(nproc)
				'';
			installPhase = ''
				mkdir -p $out/bin
				cp build/qemu-x86_64 $out/bin/risotto-nofence
			'';

			patches = [ rv_patch ];
		};
		risotto-tso = native_pkgs.stdenv.mkDerivation {
			name = "risotto-tso";
			src = builtins.fetchGit {
				url = "https://github.com/rgouicem/qemu.git";
				rev = "0344400788107ed1d4e83c7e313a2d2c5236614d";
				ref = "tcg-tso";
				submodules = true;
			};
			buildInputs = with native_pkgs; [ perl ninja meson flex bison zlib gnumake python3 pkg-config glib ];
			useNinjaBuildPhase = false;
			useNinjaInstallPhase = false;
			useMesonConfigurePhase = false;
			configurePhase = ''
				chmod +x ./scripts/shaderinclude.pl
				patchShebangs .
				./configure --target-list=x86_64-linux-user
			'';
			buildPhase = ''
				make -j$(nproc)
			'';
			installPhase = ''
				mkdir -p $out/bin
				cp build/qemu-x86_64 $out/bin/risotto-tso
			'';

			patches = [ rv_patch ];
		};

		histogram_datafiles = builtins.fetchurl {
			url = "http://csl.stanford.edu/~christos/data/histogram.tar.gz";
			sha256 = "0dmd70xwvminphai6ky0frjy883vmlq2lga375ggcdqaynf9y2mq";
		};
		linear_regression_datafiles = builtins.fetchurl {
			url = "http://csl.stanford.edu/~christos/data/linear_regression.tar.gz";
			sha256 = "0wxk8ypabz21qzy1b1h356xccpkd0xsm4k4s3m72apd3mdw3s6ad";
		};
		string_match_datafiles = builtins.fetchurl {
			url = "http://csl.stanford.edu/~christos/data/string_match.tar.gz";
			sha256 = "0gv474jxwsz7imv8sq1s13kkqryvz6k9vx54wdl5h24l8im3ccx0";
		};
		reverse_index_datafiles = builtins.fetchurl {
			url = "http://csl.stanford.edu/~christos/data/reverse_index.tar.gz";
			sha256 = "1c2zgwl7fsf4bx295cy3625a7jgc16kh8xkybpryrwhjl78wgysj";
		};
		word_count_datafiles = builtins.fetchurl {
			url = "http://csl.stanford.edu/~christos/data/word_count.tar.gz";
			sha256 = "0yr45csbkzd33xa4g5csf2y4rw8xww58j41amp8wza1q8z6g0iv4";
		};

		parsec_datafiles = builtins.fetchurl {
			url = "https://github.com/cirosantilli/parsec-benchmark/releases/download/3.0/parsec-3.0-input-sim.tar.gz";
			sha256 = "1lpyk446dzv2w92g18v90blvh9fv1d9gy83b8wlssz2rv19kd2rp";
		};

	in
	{
	# on x86 we plot, everywhere else we run benchmarks
	pkgs = pkgs;

	devShells = {

		default = native_pkgs.mkShell {
			src = self;
			packages = [
				native_pkgs.flamegraph
			] ++ native_pkgs.lib.optionals (system != "x86_64-linux") [
				qemu
				risotto-qemu
				risotto
				risotto-nofence
				risotto-tso
				native_pkgs.flamegraph
			] ++ native_pkgs.lib.optionals (system == "x86_64-linux") [
				(native_pkgs.python3.withPackages (python-pkgs: [
				python-pkgs.pandas
				python-pkgs.seaborn
				python-pkgs.matplotlib
				python-pkgs.notebook
				]))];
		};
	};

	phoenix =
		pkgs.llvmPackages_15.stdenv.mkDerivation {
			name = "phoenix";
			hardeningDisable = [ "all" ];

			src = phoenix-src;
			nativeBuildInputs = [
				pkgs.gnumake
				pkgs.binutils
			];

			configurePhase = "cd phoenix-2.0";
			buildPhase = "make";
			installPhase = ''
				mkdir $out;
				for p in $(ls tests/*/*); do
					if [[ -x $p ]]; then cp $p $out/; fi;
				done;
				ln -s ${pkgs.llvmPackages_15.stdenv.cc.libc}/lib/libc.so $out/libc.so;
                ln -s ${toString ((builtins.elemAt (builtins.filter (x: x.pname=="libunwind") pkgs.llvmPackages_15.stdenv.cc.depsTargetTargetPropagated) 0).out.outPath)}/lib/libunwind.so $out/libunwind.so;
				cd $out;
				tar -xzf ${histogram_datafiles};
				tar -xzf ${linear_regression_datafiles};
				tar -xzf ${string_match_datafiles};
				tar -xzf ${reverse_index_datafiles};
				tar -xzf ${word_count_datafiles};
			'';
		};
    phoenix_src =
        pkgs.stdenv.mkDerivation {
            name = "phoenix-16";
            src = phoenix-16;
            phases = [ "unpackPhase" "installPhase" ];
            installPhase = ''
                mkdir -p $out;
                cp -r $src/* $out;
            '';
        };
	risotto-bench =
		pkgs.llvmPackages_15.stdenv.mkDerivation {
			name = "risotto";
			hardeningDisable = [ "all" ];

			src = risotto-bench;
			nativeBuildInputs = [
				pkgs.gnumake
				pkgs.binutils
                pkgs.python3
                pkgs.sqlite
			];

            buildInputs = [
                pkgs.sqlite
            ];

            NIX_CFLAGS_COMPILE = "-I${pkgs.sqlite.dev}/include";
            NIX_LDFLAGS = "-L${pkgs.sqlite}/lib";

			configurePhase = ''
                cd benchmarks/sqlite-bench
            '';
			buildPhase = "make LDFLAGS=${pkgs.sqlite}/lib/libsqlite3.so";
			installPhase = ''
                python3 gen-sql.py

				mkdir $out;
                if [[ -x $p ]]; then cp $p $out/; fi;
                cp sqlite-bench.x86_64 -t $out;
				ln -s ${pkgs.llvmPackages_15.stdenv.cc.libc}/lib/libc.so $out/libc.so;
                ln -s ${pkgs.sqlite}/lib/libsqlite3.so $out/libsqlite3.so;
                ln -s ${toString ((builtins.elemAt (builtins.filter (x: x.pname=="libunwind") pkgs.llvmPackages_15.stdenv.cc.depsTargetTargetPropagated) 0).out.outPath)}/lib/libunwind.so $out/libunwind.so;
				cd $out;
			'';
		};
	parsec =
		pkgs.stdenv.mkDerivation {
			name = "parsec";
			hardeningDisable = [ "all" ];

			src = parsec-src;
			nativeBuildInputs = [
				native_pkgs.gnumake
				#pkgs.binutils
				native_pkgs.pkg-config
				native_pkgs.m4
				native_pkgs.gettext
				native_pkgs.wget
			];
			buildInputs = [
				#pkgs.clang_15
				#pkgs.zlib
				native_pkgs.llvmPackages.openmp.dev
			];

			configurePhase = "patchShebangs ./bin; patchShebangs .;";
			# build the following benchmarks
			buildPhase = "./bin/parsecmgmt -a build -p blackscholes bodytrack ferret fluidanimate freqmine swaptions streamcluster";
			installPhase = ''
				mkdir $out;
				ln -s ${pkgs.stdenv.cc.libc}/lib/libc.so $out/libc.so;
				ln -s ${pkgs.stdenv.cc.libcxx}/lib/libc++.so.1 $out/libc++.so;
				ln -s ${pkgs.stdenv.cc.libcxx}/lib/libc++abi.so.1 $out/libc++abi.so;

				tar -xzf ${parsec_datafiles};

				for p in blackscholes bodytrack ferret fluidanimate freqmine swaptions ; do
					echo $p;
					cp pkgs/apps/$p/inst/*/bin/$p $out;

					if [[ -d parsec-3.0/pkgs/apps/$p/inputs ]]; then
						mkdir -p $out/$p\_datafiles;
						ls $out
						tar -xf parsec-3.0/pkgs/apps/$p/inputs/input_simlarge.tar -C $out/$p\_datafiles;
					fi;
				done;
				for p in streamcluster; do
					echo $p;
					cp pkgs/kernels/$p/inst/*/bin/$p $out;

					if [[ -d parsec-3.0/pkgs/kernels/$p/inputs ]]; then
						mkdir -p $out/$p\_datafiles;
						tar -xf parsec-3.0/pkgs/kernels/$p/inputs/input_simlarge.tar -C $out/$p\_datafiles;
					fi;
				done;
			'';
		};
	checks = {
		ccTest = pkgs.llvmPackages_15.stdenv.cc;
	};
	});
}
