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
		risotto-pkgs = {
			type = "github";
			owner = "NixOS";
			repo = "nixpkgs";
			rev = "80b3160c21977e627ae99f0c87404cdcd85646ad";
		};
	};

	nixConfig.extra-substituters = [ "https://musl-toolchains.cachix.org" ];
	nixConfig.extra-trusted-public-keys = [ "musl-toolchains.cachix.org-1:g9L50mmWHHMzAVIfgLVQjhoBsjT66n3LDa0f8xeigpI=" ];

	outputs = { self, nixpkgs, flake-utils, phoenix-src, risotto-pkgs, ... }:
	flake-utils.lib.eachSystem [ "x86_64-linux" "riscv64-linux" "aarch64-linux" ] (system:
	let
		pkgs = import nixpkgs { system = system; crossSystem = { config = system+"-musl"; }; config.allowUnsupportedSystem=true; };
		native_pkgs = import nixpkgs { inherit system; config.allowUnsupportedSystem=true; };
		risotto_pkgs = import risotto-pkgs { inherit system; config.allowUnsupportedSystem=true; };

		qemu = native_pkgs.qemu.override { pipewireSupport=false; hostCpuTargets=["x86_64-linux-user"]; jackSupport=false; alsaSupport=false; gtkSupport=false; vncSupport=false; pulseSupport=false; smartcardSupport=false; spiceSupport=false; glusterfsSupport=false; openGLSupport=false; sdlSupport=false; usbredirSupport=false; xenSupport=false; cephSupport=false; virglSupport=false;};
		risotto_base = risotto_pkgs.qemu.override { hostCpuTargets=["x86_64-linux-user"]; jackSupport=false; alsaSupport=false; gtkSupport=false; vncSupport=false; pulseSupport=false; smartcardSupport=false; spiceSupport=false; glusterfsSupport=false; openGLSupport=false; sdlSupport=false; usbredirSupport=false; xenSupport=false; cephSupport=false; virglSupport=false;};
		risotto-qemu = risotto_base.overrideAttrs (oldAttrs: {
			name = "risotto-qemu";
			src = builtins.fetchGit {
				url = "https://github.com/rgouicem/qemu.git";
				rev = "a73de0b0c1190af7d7e5e8ed73b4eb780acde2c5";
				ref = "master-6.1.0";
				submodules = true;
			};
		});
		risotto = risotto_base.overrideAttrs (oldAttrs: {
			name = "risotto";
			src = builtins.fetchGit {
				url = "https://github.com/rgouicem/qemu.git";
				rev = "1fca2ddf96cc78cf6dd9dab4edf7a354718b08de";
				ref = "risotto";
				submodules = true;
			};
		});
		risotto-nofence = risotto_base.overrideAttrs (oldAttrs: {
			name = "risotto-nofence";
			src = builtins.fetchGit {
				url = "https://github.com/rgouicem/qemu.git";
				rev = "f31902ed15b2b86bac4f06f7899919be1c649ea9";
				ref = "no-fences";
				submodules = true;
			};
		});
		risotto-tso = risotto_base.overrideAttrs (oldAttrs: {
			name = "risotto-tso";
			src = builtins.fetchGit {
				url = "https://github.com/rgouicem/qemu.git";
				rev = "0344400788107ed1d4e83c7e313a2d2c5236614d";
				ref = "tcg-tso";
				submodules = true;
			};
		});

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
	in
	{
	devShell =
		native_pkgs.mkShell.override { stdenv = native_pkgs.llvmPackages_15.stdenv; } {
			packages = [
				qemu
				risotto
				risotto-qemu
				risotto-nofence
				risotto-tso
			] ++ native_pkgs.lib.optionals (system == "x86_64-linux") [
				(native_pkgs.python3.withPackages (python-pkgs: [
											python-pkgs.pandas
											python-pkgs.seaborn
											python-pkgs.matplotlib
											python-pkgs.notebook
				]))];
			shellHook = ''
				export QEMU_INI=${
				(native_pkgs.writeTextFile {
					name = "qemu.ini";
					text = ''
[qemu]
LATEST_QEMU=${qemu}/bin/qemu-x86_64
RISOTTO_QEMU=${risotto-qemu}/bin/qemu-x86_64
RISOTTO=${risotto}/bin/qemu-x86_64
RISOTTO_NF=${risotto-nofence}/bin/qemu-x86_64
RISOTTO_TSO=${risotto-tso}/bin/qemu-x86_64
					'';
				})}
				'';
		};
	
	phoenix =
		pkgs.llvmPackages_15.stdenv.mkDerivation {
			name = "phoenix";
			hardeningDisable = [ "all" ];

			src = phoenix-src;
			nativeBuildInputs = [
				pkgs.gnumake
				#pkgs.llvmPackages_15.bintools
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
				cd $out;
				tar -xzf ${histogram_datafiles};
				tar -xzf ${linear_regression_datafiles};
				tar -xzf ${string_match_datafiles};
				tar -xzf ${reverse_index_datafiles};
				tar -xzf ${word_count_datafiles};
			'';
		};
	checks = {
		ccTest = pkgs.llvmPackages_15.stdenv.cc;
	};
	});
}
