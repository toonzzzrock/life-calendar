{ pkgs, ... }:

{
  languages.cplusplus.enable = true;

  packages = with pkgs; [
    cmake
    pkg-config
    gnumake
  ];
}
