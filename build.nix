{ callPackage
, symlinkJoin
, fetchurl
, makeDesktopItem
, copyDesktopItems
, hl2-unwrapped ? callPackage ./hl2-unwrapped.nix { }
, hl2-wrapper ? callPackage ./hl2-wrapper.nix { inherit hl2-unwrapped; }
}:

let
  name = "hl2";

  icon = fetchurl {
    url = "https://upload.wikimedia.org/wikipedia/commons/1/14/Half-Life_2_Logo.svg";
    hash = "sha256-/4GlYVAUSZiK7eLjM/HymcGphk5s2uCPOQuB1XOstuI=";
  };
in
symlinkJoin rec{
  inherit name;

  paths = [ 
    hl2-unwrapped
    hl2-wrapper
  ] ++ desktopItems;

  nativeBuildInputs = [
    copyDesktopItems
  ];

  postBuild = ''
    ln -s $out/bin/hl2-wrapper $out/bin/${name}

    install -Dm444 ${icon} $out/share/icons/hicolor/scalable/apps/${name}.svg
  '';

  desktopItems = [
    (makeDesktopItem {
      name = "${name}";
      exec = "${name}";
      icon = "${name}";
      desktopName = "Half-Life 2";
      comment = "Built from the leaked source code";
      categories = [ "Game" ];
    })
  ];
}
