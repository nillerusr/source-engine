{
  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs";
    flake-utils.url = "github:numtide/flake-utils";

    ivp-submodule = {
      url = "github:nillerusr/source-physics";
      flake = false;
    };

    lib-submodule = {
      url = "github:nillerusr/source-engine-libs";
      flake = false;
    };

    thirdparty-submodule = {
      url = "github:nillerusr/source-thirdparty";
      flake = false;
    };
  };

  outputs = { self, nixpkgs, flake-utils, ... }@inputs:
  flake-utils.lib.eachDefaultSystem (system:
    let
      pkgs = import nixpkgs { inherit system; };

      # Fix for git submodules
      hl2-unwrapped = (pkgs.callPackage ./hl2-unwrapped.nix { }).overrideAttrs {
        postPatch = ''
          rm -rf ./{ivp,lib,thirdparty}
          ln -s ${inputs.ivp-submodule} ./ivp
          ln -s ${inputs.lib-submodule} ./lib
          ln -s ${inputs.thirdparty-submodule} ./thirdparty
        '';
      };
    in
    {
      # Install the game on your own computer
      packages = {
        hl2 = pkgs.callPackage ./build.nix { inherit hl2-unwrapped; };
        default = self.packages.${system}.hl2;
      };

      # Build the game for others
      devShells.default = pkgs.mkShell rec {
        nativeBuildInputs = with pkgs; [
          makeWrapper
          SDL2
          freetype
          fontconfig
          zlib
          bzip2
          libjpeg
          libpng
          curl
          openal
          libopus
          pkg-config
          gcc
          python3
        ];
      };
    }
  );
}
