{
  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";

  outputs = { nixpkgs, ... }: {
    devShell.aarch64-linux = let
      pkgs = import nixpkgs { system = "aarch64-linux"; };
    in pkgs.mkShell {
      packages = [
        # We may remove clang, if we use g++ instead
        # pkgs.clang_18
        #llvmPackages_18.compiler-rt
      ];
    };

    devShell.riscv64-linux = let
      pkgs = import nixpkgs { system = "riscv64-linux"; };
    in pkgs.mkShell {
      packages = [
        # We may remove clang, if we use g++ instead
        # pkgs.clang_18
        #llvmPackages_18.compiler-rt
      ];
    };
  };
}
