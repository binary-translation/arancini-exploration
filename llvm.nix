{ pkgs }:

pkgs.llvmPackages_16.override ({
  monorepoSrc = pkgs.fetchFromGitHub {
    owner = "martin-fink";
    repo = "llvm-project";
    rev = "f719dba79a56626874c04c7e9d3d77b66d83de5f";
    sha256 = "sha256-CjUN6nyjYW3v7rfDF1uo5gcmPqQIkOhdgD6WTzQ4X5g=";
  };
  gitRelease = {
    version = "16.0.6";
    rev-version = "16.0.6-arancini";
  };
  officialRelease = null;
})


