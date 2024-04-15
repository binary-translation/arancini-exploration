{ pkgs ? import ../../nixpkgs {} }:
let
my-python-packages = ps: with ps; [
	pandas
	#jupyter
	numpy
];
my-python = pkgs.python3.withPackages my-python-packages;
in
pkgs.mkShell rec {
	buildInputs = with pkgs; [
		my-python
		qemu
	];

	shellHook = ''
		export ARANCINI_ROOT=$(realpath ..)
	'';
}
