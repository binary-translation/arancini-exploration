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
	in
	{
    # artifact = native_pkgs.dockerTools.buildLayeredImage {
    #   name = "arancini-${system}";
    #   tag = "latest";
    #   fromImage = native_pkgs.dockerTools.pullImage {
    #     imageName = "nixos/nix";
    #     sha256 = "0opkJTUNNdz+Ve8gHlJXJ0VsatzBjLYL8xu/ekNc5Co=";
    #     imageDigest = "sha256:081b65e50a5c4e6ef4a9094a462da3b83ff76bfec70236eb010047fcee36e11c";
    #   };

    #   contents = [
    #     self.outputs.defaultPackage.${system}
    #     # !!! make sure to nix copy this to the builder,
    #     # otherwise it tries to cross-compile the whole toolchain
    #     # benchmarks.phoenix.x86_64-linux
    #     # benchmarks.phoenix.${system}
    #     # benchmarks.risotto-qemu.${system}
    #     # benchmarks.risotto.${system}
    #     # benchmarks.risotto-nofence.${system}
    #     # benchmarks.risotto-tso.${system}
    #     # native_pkgs.gdb
    #     native_pkgs.clang_18
    #     # native_pkgs.gcc
    #     native_pkgs.dockerTools.binSh
    #   ];

    #   config = {
    #     Cmd = [ "${native_pkgs.bash}/bin/bash"];
    #   };
    # };
		defaultPackage = native_pkgs.stdenv.mkDerivation {
				name = "arancini";
				pname = "txlat";
				src = self;
				nativeBuildInputs = with native_pkgs; [
					gdb
					python3
					cmake
					pkg-config
					gcc
					m4
                    flex
                    bison
					clang_18
          util-linux
				];
				buildInputs = with native_pkgs; [
          fmt
					zlib
					boost
					patched-xed
					libffi
					fadec
					libxml2
					llvmPackages_18.llvm.dev
					llvmPackages_18.bintools
					llvmPackages_18.lld
					flex
				] ++ native_pkgs.lib.optionals ( system == "aarch64-linux" ) [ native_pkgs.keystone ];
				depsTargetTarget = [ native_pkgs.gcc ];
				configurePhase = ''
					export FLAKE_BUILD=1
					export NDEBUG=1
					cmakeConfigurePhase

          mkdir -p $out/phoenix-x86_64
          cp -r $src/phoenix-x86_64/* $out/phoenix-x86_64
          chmod +x $out/phoenix-x86_64/*

          cp $src/init_lib.c $out/
          cp $src/lib.*.lds $out/

          cp $src/*exec.lds $out/
				'';
				cmakeFlags = [ "-DBUILD_TESTS=1" ];
        dontStrip = true;
			};
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
			native_pkgs = import nixpkgs { system = "x86_64-linux"; };
			remote_pkgs = import nixpkgs { system = "x86_64-linux"; crossSystem = system; };
		in
		{
		crossPackage = native_pkgs.stdenv.mkDerivation {
				name = "arancini";
				pname = "txlat";
				src = self;
				nativeBuildInputs = [
					native_pkgs.gdb
					native_pkgs.python3
					native_pkgs.cmake
					native_pkgs.pkg-config
					native_pkgs.m4
                    native_pkgs.flex
                    native_pkgs.bison
					native_pkgs.gcc
				];
				buildInputs = [
                    native_pkgs.fmt
					native_pkgs.zlib
					native_pkgs.boost
					patched-xed
					native_pkgs.libffi
					fadec
					native_pkgs.libxml2
					native_pkgs.llvmPackages_18.llvm.dev
					native_pkgs.llvmPackages_18.bintools
					native_pkgs.llvmPackages_18.lld
                    native_pkgs.flex
				] ++ native_pkgs.lib.optionals ( system == "aarch64-linux" ) [ native_pkgs.keystone ];
				depsTargetTarget = [ remote_pkgs.gcc ];
				configurePhase = ''
					export FLAKE_BUILD=1
					export NDEBUG=1
					cmakeConfigurePhase
				'';
				cmakeFlags = [
					"-DBUILD_TESTS=1"
					"-DCMAKE_BUILD_TYPE=Release"
				] ++ native_pkgs.lib.optionals (system == "riscv64-linux") [
					"-DDBT_ARCH=RISCV64"
				] ++ native_pkgs.lib.optionals (system == "aarch64-linux") [
					"-DDBT_ARCH=AARCH64"
				];
				fixupPhase = ''
					ln -s ${remote_pkgs.gcc.outPath}/bin/g++ $out/cross-g++;
				'';
			};
	});
}
