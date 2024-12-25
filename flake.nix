{
  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs";

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

  outputs = { self, nixpkgs, ... }@inputs:
  let
    pkgs = nixpkgs.legacyPackages.x86_64-linux;

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
    packages.x86_64-linux = {
      hl2 = pkgs.callPackage ./build.nix { inherit hl2-unwrapped; };
      default = self.packages.x86_64-linux.hl2;
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
  };
}
