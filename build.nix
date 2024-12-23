{ callPackage
, symlinkJoin
}:

let
  # Strangely, these are overridable
  hl2-unwrapped = callPackage ./hl2-unwrapped.nix { };
  hl2-wrapper = callPackage ./hl2-wrapper.nix { inherit hl2-unwrapped; };
in
symlinkJoin {
  name = "hl2";

  paths = [ 
    hl2-unwrapped
    hl2-wrapper
  ];

  postBuild = "ln -s $out/bin/hl2-wrapper $out/bin/hl2";
}
