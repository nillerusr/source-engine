{ stdenv
, python3
, wafHook
, callPackage
, SDL2
, freetype
, fontconfig
, zlib
, bzip2
, libjpeg
, libpng
, curl
, openal
, libopus
, pkg-config
, gcc
, extraWafConfigureFlags ? [ ]
}:

stdenv.mkDerivation {
  pname = "hl2";
  version = "1.0";

  src = ./.;

  nativeBuildInputs = [
    python3
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
    wafHook
  ];

  wafConfigureFlags = [
    "-T fastnative"
    "--disable-warns"
    "--build-flags=-Wno-error=format-security"
  ] ++ extraWafConfigureFlags;
}
