{
  description = "spell compiler devShell";

  inputs.nixpkgs.url = "github:nixos/nixpkgs/nixos-unstable";

  outputs = { self, nixpkgs }:
    let
      system = "x86_64-linux";
      pkgs = nixpkgs.legacyPackages.${system};
    in
    {
      devShells.${system}.default = pkgs.mkShell {
        packages = with pkgs; [
          gcc
          gnumake
          clang-tools
          python3
        ];

        shellHook = ''
          echo "spell devShell: gcc, gnumake, clang-tools"
        '';
      };
    };
}
