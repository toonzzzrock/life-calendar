{
  description = "Life Calendar - A TUI life calendar application";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    devenv.url = "github:cachix/devenv";
    systems.url = "github:nix-systems/default";
  };

  outputs =
    {
      self,
      nixpkgs,
      devenv,
      systems,
      ...
    }@inputs:
    let
      forEachSystem = nixpkgs.lib.genAttrs (import systems);
    in
    {
      packages = forEachSystem (
        system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
        in
        {
          life-calendar = pkgs.stdenv.mkDerivation {
            pname = "life-calendar";
            version = "0.1.0";
            src = ./.;

            nativeBuildInputs = with pkgs; [
              cmake
              pkg-config
              makeWrapper
              git
            ];

            buildInputs = with pkgs; [ ftxui ];

            cmakeFlags = [
              "-DCMAKE_BUILD_TYPE=Release"
            ];

            postInstall = ''
              mkdir -p $out/share/life-calendar
              cp assets/template.md $out/share/life-calendar/template.md

              wrapProgram $out/bin/life-calendar \
                --set-default LIFE_CALENDAR_BIRTH_DATE "2000-01-01" \
                --set-default LIFE_CALENDAR_DEATH_DATE "2080-10-19" \
                --set-default LIFE_CALENDAR_DIARY_DIR "~/.life-calendar/diary" \
                --set-default LIFE_CALENDAR_DIARY_TEMPLATE "$out/share/life-calendar/template.md"
            '';

            meta = with pkgs.lib; {
              description = "A TUI life calendar application";
              license = licenses.mit;
              platforms = platforms.unix;
              mainProgram = "life-calendar";
            };
          };
          default = self.packages.${system}.life-calendar;
        }
      );

      nixosModules.default = { config, lib, pkgs, ... }:
        let
          cfg = config.programs.life-calendar;
        in
        {
          options.programs.life-calendar = {
            enable = lib.mkEnableOption "Life Calendar TUI";
            birthDate = lib.mkOption {
              type = lib.types.str;
              default = "2000-01-01";
              description = "Your birth date in YYYY-MM-DD format.";
            };
            deathDate = lib.mkOption {
              type = lib.types.str;
              default = "2080-01-01";
              description = "Your estimated death date in YYYY-MM-DD format.";
            };
            editor = lib.mkOption {
              type = lib.types.str;
              default = "nvim";
              description = "The editor to use for diary entries.";
            };
            diaryDir = lib.mkOption {
              type = lib.types.str;
              default = "~/.life-calendar/diary";
              description = "Directory where diary entries are stored.";
            };
            diaryTemplate = lib.mkOption {
              type = lib.types.str;
              default = "";
              description = "Path to a template file for new diary entries.";
            };
          };

          config = lib.mkIf cfg.enable {
            environment.systemPackages = [
              (pkgs.symlinkJoin {
                name = "life-calendar-configured";
                paths = [ self.packages.${pkgs.system}.default ];
                nativeBuildInputs = [ pkgs.makeWrapper ];
                postBuild = ''
                  wrapProgram $out/bin/life-calendar \
                    --set LIFE_CALENDAR_BIRTH_DATE "${cfg.birthDate}" \
                    --set LIFE_CALENDAR_DEATH_DATE "${cfg.deathDate}" \
                    --set LIFE_CALENDAR_EDITOR "${cfg.editor}" \
                    --set LIFE_CALENDAR_DIARY_DIR "${cfg.diaryDir}" \
                    --set LIFE_CALENDAR_DIARY_TEMPLATE "${if cfg.diaryTemplate != "" then cfg.diaryTemplate else "${self.packages.${pkgs.system}.default}/share/life-calendar/template.md"}"
                '';
              })
            ];
          };
        };

      devShells = forEachSystem (
        system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
        in
        {
          default = devenv.lib.mkShell {
            inherit inputs pkgs;
            modules = [
              ./devenv.nix
            ];
          };
        }
      );
    };
}
