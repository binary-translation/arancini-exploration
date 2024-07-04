{ pkgs }:

pkgs.llvmPackages_16.override ({
  monorepoSrc = pkgs.fetchFromGitHub {
    owner = "martin-fink";
    repo = "llvm-project";
    rev = "73b889f65c62e94f78def38bd2dac07a9b819a70";
    sha256 = "sha256-hdVMRrdgnz03ooGGU3pnUQjp1wJ4LNa1sjlDeRaTnb8=";
  };
  gitRelease = {
    version = "16.0.6";
    rev = "73b889f65c62e94f78def38bd2dac07a9b819a70";
    rev-version = "16.0.6-arancini";
  };
  officialRelease = null;
})


