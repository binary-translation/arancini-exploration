{
	description = "Arancini dev shell";

	# To update flake.lock to the latest nixpkgs: `nix flake update`
	inputs = {
		nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
		flake-utils.url = "github:numtide/flake-utils";
	};

	# output format guide https://nixos.wiki/wiki/Flakes#Output_schema
	outputs = { self, nixpkgs, flake-utils, ... }:
	flake-utils.lib.eachDefaultSystem (system:
	let
		pkgs = nixpkgs.legacyPackages.${system};
	in
	{
		defaultPackage =
			with import nixpkgs { system = "x86_64-linux"; };
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
