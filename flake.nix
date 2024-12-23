{
  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs";
  };

  outputs = { self, nixpkgs }:
  let
    pkgs = nixpkgs.legacyPackages.x86_64-linux;
  in
  {
    packages.x86_64-linux = {
      hl2 = pkgs.callPackage ./build.nix { };
      default = self.packages.x86_64-linux.hl2;
    };
  };
}
