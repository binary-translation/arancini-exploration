{
	description = "Arancini dev shell";

	# To update flake.lock to the latest nixpkgs: `nix flake update`
	inputs = {
		nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
		flake-utils.url = "github:numtide/flake-utils";
	};

	# output format guide https://nixos.wiki/wiki/Flakes#Output_schema
	outputs = { self, nixpkgs, flake-utils, ... }:
	flake-utils.lib.eachSystem ["x86_64-linux" "aarch64-linux" "riscv64-linux" ] (system:
	let
		all_pkgs =
			if system == "aarch64-linux" then
				import nixpkgs { inherit system; overlays = [
					(final: prev: { xed = prev.xed.overrideAttrs (oldAttr: rec {
						buildPhase = ''
							patchShebangs mfile.py
							# this will build, NOT test and install
							./mfile.py --prefix $out
						''; });
					})];}
			else
				import nixpkgs { inherit system; };
		pkgs =
			if system == "riscv64-linux" then
				nixpkgs.legacyPackages.x86_64-linux.pkgsCross.riscv64
			else
				all_pkgs;
	in
	{
		defaultPackage =
			with pkgs;
			stdenv.mkDerivation {
				name = "arancini";
				src = self;
				nativeBuildInputs = [
					zlib
					boost
					xed
					libffi
					graphviz
					gdb
					libxml2
					bear
					python3	
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
			};
	});
}
