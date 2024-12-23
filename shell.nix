{ pkgs ? import <nixpkgs> {} }:
pkgs.mkShell rec {
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
  ];
  buildInputs = [
    (pkgs.python3.withPackages (ps: with ps; [
      ipython
    ]))
  ];
}
