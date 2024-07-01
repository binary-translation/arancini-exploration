{ pkgs }:

pkgs.llvmPackages_16.override ({
  monorepoSrc = pkgs.fetchFromGitHub {
    owner = "martin-fink";
    repo = "llvm-project";
    rev = "3c74260185c893bfb3a9b66de6d6845606a5ef0e";
    sha256 = "sha256-5FnRm+2E59wHM/fnfyEy22J5eVpPK+2USzAmVSKgoNo";
  };
  gitRelease = {
    version = "16.0.6";
    rev = "3c74260185c893bfb3a9b66de6d6845606a5ef0e";
    rev-version = "16.0.6-arancini";
  };
  officialRelease = null;
})


