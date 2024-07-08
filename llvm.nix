{ pkgs }:

pkgs.llvmPackages_16.override ({
  monorepoSrc = pkgs.fetchFromGitHub {
    owner = "martin-fink";
    repo = "llvm-project";
    rev = "ed84a56b98fe3de443d47a71630901dad01ee820";
    sha256 = "sha256-PiwfcZP+9K3TDUoMnoTgugEuw2t7sUiqYlg+H6yyF68=";
  };
  gitRelease = {
    version = "16.0.6";
#    rev = "568e4ae228fc2f5f9c9d026ca328c59c98a63026";
    rev-version = "16.0.6-arancini";
  };
  officialRelease = null;
})


