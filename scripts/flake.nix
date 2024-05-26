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
	};

	nixConfig.extra-substituters = [ "https://musl-toolchains.cachix.org" ];
	nixConfig.extra-trusted-public-keys = [ "musl-toolchains.cachix.org-1:g9L50mmWHHMzAVIfgLVQjhoBsjT66n3LDa0f8xeigpI=" ];

	outputs = { self, nixpkgs, flake-utils, phoenix-src, ... }:
	flake-utils.lib.eachSystem [ "x86_64-linux" "riscv64-linux" "aarch64-linux" ] (system:
	let
		pkgs = import nixpkgs { system = system; crossSystem = { config = system+"-musl"; }; };

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
		pkgs.mkShell.override { stdenv = pkgs.llvmPackages_15.stdenv; } {
			packages = [];
		};
	
	phoenix =
		pkgs.llvmPackages_15.stdenv.mkDerivation {
			name = "phoenix";

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
