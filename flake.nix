{
	description = "Arancini dev shell";

	# To update flake.lock to the latest nixpkgs: `nix flake update`
	inputs = {
		nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
		flake-utils.url = "github:numtide/flake-utils";
		xed-src = {
			url = "github:intelxed/xed";
			flake = false;
		};
		mbuild-src = {
			url = "github:intelxed/mbuild";
			flake = false;
		};
		fadec-src = {
			url = "github:aengelke/fadec";
			flake = false;
		};
	};

	nixConfig.extra-substituters = [
		"https://cache.garnix.io"
		"https://tum-dse.cachix.org"
	];
	nixConfig.extra-trusted-public-keys = [
		"cache.garnix.io:CTFPyKSLcx5RMJKfLo5EEPUObbA78b0YQ2DTCJXqr9g="
	];

	# output format guide https://nixos.wiki/wiki/Flakes#Output_schema
	outputs = { self, nixpkgs, flake-utils, xed-src, mbuild-src, fadec-src, ... }:
	flake-utils.lib.eachSystem ["x86_64-linux" "aarch64-linux" "riscv64-linux" ] (system:
	let
		my-mbuild = 
			with pkgs;
			python3Packages.buildPythonPackage rec {
				pname = "mbuild";
				version = "2022.07.28";

				src = mbuild-src;
				patches = [ ./mbuild-riscv.patch ];
			};
		patched-xed = 
			with pkgs;
			stdenv.mkDerivation {
				pname = "xed";
				version = "2022.08.11";

				src = xed-src;
				nativeBuildInputs = [ my-mbuild gcc ];

				buildPhase = ''
				    patchShebangs mfile.py

					# this will build, test and install
				    ./mfile.py --prefix $out
				'';

				dontInstall = true; # already installed during buildPhase
			};
		fadec =
			with pkgs;
			stdenv.mkDerivation {
				name = "fadec";
				src = fadec-src;
				nativeBuildInputs = [ meson ninja ];
			};
		all_pkgs =
				import nixpkgs { inherit system; };
		pkgs =
			if system == "riscv64-linux" then
				(import nixpkgs { system = "x86_64-linux"; crossSystem.config = "riscv64-unknown-linux-gnu"; }).pkgsCross.riscv64
			else
				all_pkgs;
	in
	{
		defaultPackage =
			with pkgs;
			stdenv.mkDerivation {
				name = "arancini";
				pname = "txlat";
				src = self;
				nativeBuildInputs = [
					zlib
					boost
					patched-xed
					libffi
					graphviz
					gdb
					libxml2
					python3
					fadec
				];
				buildInputs = [
					cmake
					pkg-config
					llvmPackages_14.llvm.dev
					llvmPackages_14.bintools
					llvmPackages_14.lld
					clang_14
					git
				];
				configurePhase = ''
					export FLAKE_BUILD=1
					cmakeConfigurePhase
				'';
				cmakeFlags = [ "-DBUILD_TESTS=1" ];
			};
	});
}
