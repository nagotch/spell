{
  description = "spell compiler devShell";

  inputs.nixpkgs.url = "github:nixos/nixpkgs/nixos-unstable";

  outputs = { self, nixpkgs }:
    let
      system = "x86_64-linux";
      pkgs = nixpkgs.legacyPackages.${system};
    in
    {
      # spell 本体と spell-lsp。prelude.sp は実行ファイル相対で探すため、
      # バイナリと同じ $out/bin へ一緒に入れる。
      # 実装は Rust（src/Cargo.toml）。依存クレートはないため、
      # オフラインのまま cargo でビルドできる。
      packages.${system}.default = pkgs.stdenv.mkDerivation {
        pname = "spell";
        version = "0.1.0";
        src = ./.;

        nativeBuildInputs = with pkgs; [
          cargo
          rustc
        ];

        buildPhase = ''
          export CARGO_HOME=$TMPDIR/cargo-home
          cargo build --release --manifest-path src/Cargo.toml --offline --locked
        '';

        installPhase = ''
          mkdir -p $out/bin
          cp src/target/release/spell src/target/release/spell-lsp src/prelude.sp $out/bin/
          cp tools/nasm2gas.py $out/bin/
          sed -i "1s|.*|#!${pkgs.python3}/bin/python3|" $out/bin/nasm2gas.py
          chmod +x $out/bin/nasm2gas.py
        '';
      };

      devShells.${system}.default = pkgs.mkShell {
        packages = with pkgs; [
          gcc
          gnumake
          clang-tools
          python3
          bear
          cargo
          rustc
        ];

        shellHook = ''
        '';
      };
    };
}
