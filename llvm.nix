{ pkgs }:

pkgs.llvmPackages_16.override ({
  monorepoSrc = pkgs.fetchFromGitHub {
    owner = "martin-fink";
    repo = "llvm-project";
    rev = "568e4ae228fc2f5f9c9d026ca328c59c98a63026";
    sha256 = "sha256-3ZCoog5Nwm1bBH9HBraa9c7yzhcSVM3C5Y9MKrPQS38=";
  };
  gitRelease = {
    version = "16.0.6";
    rev = "568e4ae228fc2f5f9c9d026ca328c59c98a63026";
    rev-version = "16.0.6-arancini";
  };
  officialRelease = null;
})


