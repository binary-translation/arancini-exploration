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
			native_pkgs.python3Packages.buildPythonPackage {
				pname = "mbuild";
				version = "2022.07.28";

				src = mbuild-src;
				patches = [ ./mbuild-riscv.patch ];
			};
		patched-xed = native_pkgs.callPackage(
        {stdenv, lib}:
			stdenv.mkDerivation {
				pname = "xed";
				version = "2022.08.11";

				src = xed-src;
				nativeBuildInputs = [ my-mbuild ];

				buildPhase = ''
				    patchShebangs mfile.py

					# this will build, test and install
				    ./mfile.py --prefix $out'';

				dontInstall = true; # already installed during buildPhase
			}){};
		fadec = native_pkgs.callPackage(
           {stdenv, meson, ninja}:
			stdenv.mkDerivation {
				name = "fadec";
				src = fadec-src;
				nativeBuildInputs = [ meson ninja ];
			}){};
		native_pkgs = import nixpkgs { system = system; };
        aranciniLlvmPackages = import ./llvm.nix { pkgs = native_pkgs; };
	in
	{
		defaultPackage = native_pkgs.callPackage(
		{stdenv, graphviz, gdb, python3, valgrind, git, cmake, pkg-config, clang, zlib, boost, libffi, libxml2,
		 lib, gcc, fmt, pkgsCross, m4, keystone, flex, bison}:
			stdenv.mkDerivation {
				name = "arancini";
				pname = "txlat";
				src = self;
				nativeBuildInputs = [
					#graphviz
					gdb
					python3
					#valgrind
					git
					cmake
					pkg-config
					clang
					gcc
					m4
                    flex
                    bison
				];
				buildInputs = [
                    fmt
					zlib
					boost
					patched-xed
					libffi
					fadec
					libxml2
					aranciniLlvmPackages.llvm.dev
					aranciniLlvmPackages.bintools
					aranciniLlvmPackages.lld
					flex
				] ++ lib.optionals ( system == "aarch64-linux" ) [ keystone ];
				depsTargetTarget = [ gcc ];
				configurePhase = ''
					export FLAKE_BUILD=1
					export NDEBUG=1
					cmakeConfigurePhase
				'';
				cmakeFlags = [ "-DBUILD_TESTS=1" ];
			}
		) {};
	}) //
	flake-utils.lib.eachSystem [ "aarch64-linux" "riscv64-linux" ] (system:
	let
		my-mbuild =
			native_pkgs.python3Packages.buildPythonPackage {
				pname = "mbuild";
				version = "2022.07.28";

					src = mbuild-src;
					patches = [ ./mbuild-riscv.patch ];
				};
			patched-xed = build_pkgs.callPackage(
    	    {stdenv, lib}:
				stdenv.mkDerivation {
					pname = "xed";
					version = "2022.08.11";

					src = xed-src;
					nativeBuildInputs = [ my-mbuild ];

					buildPhase = ''
					    patchShebangs mfile.py

						# this will build, test and install
					    ./mfile.py --prefix $out'' + lib.optionalString (system == "riscv64-linux") " --toolchain riscv64-linux-gnu- --host-cpu riscv64" + lib.optionalString (system == "aarch64-linux") " --toolchain aarch64-linux-gnu- --host-cpu aarch64";

					dontInstall = true; # already installed during buildPhase
				}){};
			fadec = build_pkgs.callPackage(
    	       {stdenv, meson, ninja}:
				stdenv.mkDerivation {
					name = "fadec";
					src = fadec-src;
					nativeBuildInputs = [ meson ninja ];
				}){};
			native_pkgs = import nixpkgs { system = "x86_64-linux"; };
			build_pkgs = import nixpkgs { system = "x86_64-linux"; crossSystem.config = system+"-gnu"; };
			aranciniLlvmPackages = import ./llvm.nix { pkgs = build_pkgs; };
    	in
		{
		crossPackage = build_pkgs.callPackage(
		{stdenv, graphviz, gdb, python3, valgrind, git, cmake, pkg-config, clang, zlib, boost, libffi, libxml2,
			lib, gcc, fmt, pkgsCross, m4, keystone, flex, bison}:
			stdenv.mkDerivation {
				name = "arancini";
				pname = "txlat";
				src = self;
				nativeBuildInputs = [
					#graphviz
					gdb
					python3
					#valgrind
					git
					cmake
					pkg-config
					clang
					m4
                    flex
                    bison
				];
				buildInputs = [
                    fmt
					zlib
					boost
					patched-xed
					libffi
					fadec
					libxml2
					aranciniLlvmPackages.llvm.dev
					aranciniLlvmPackages.bintools
					aranciniLlvmPackages.lld
                    flex
				] ++ lib.optionals ( system == "aarch64-linux" ) [ keystone ];
				depsTargetTarget = [ gcc ];
				configurePhase = ''
					export FLAKE_BUILD=1
					export NDEBUG=1
					cmakeConfigurePhase
				'';
				cmakeFlags = [ "-DBUILD_TESTS=1" "-DCMAKE_BUILD_TYPE=Release" ] ++ lib.optionals (system == "riscv64-linux") ["--toolchain riscv64-toolchain-nix.cmake"] ++ lib.optionals (system == "aarch64-linux") ["--toolchain aarch64-toolchain-nix.cmake"];
			}
		) {};
	});
}
