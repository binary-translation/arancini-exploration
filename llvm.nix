{ pkgs }:

pkgs.llvmPackages_16.override ({
  monorepoSrc = pkgs.fetchFromGitHub {
    owner = "martin-fink";
    repo = "llvm-project";
    rev = "bd1ab15fe43718c5a8388887e140b9b6340db578";
    sha256 = "sha256-HfENJS+h0VkMVGIwt4fQ4YKaq4vbxJgjcI6Soxkl52c=";
  };
  gitRelease = {
    version = "16.0.6";
    rev-version = "16.0.6-arancini";
  };
  officialRelease = null;
})

