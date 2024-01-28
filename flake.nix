# TODO: Consider using mold (linker)
#TODO: Add declarative drawpile server support
#TODO: Consider my terrible life choices that led me to derive os-es from config files
{
  description = "Collaborative drawing program that lets multiple people draw.";

  inputs = {
    #main packages
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

    #utilities in writing flakes
    flake-utils.url = "github:numtide/flake-utils";

    #compatibility with non flaked nix
    flake-compat.url = "https://flakehub.com/f/edolstra/flake-compat/1.tar.gz";
  };

  outputs = { self, nixpkgs, flake-utils, ... }:
    #Support x86_64/arm linux
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = nixpkgs.legacyPackages.${system};

        #Compile time libs

        mkNativeInputs = { qt5 }:
          let
            qtStuff = if qt5 then
              [ pkgs.libsForQt5.qt5.wrapQtAppsHook ]
            else
              [ pkgs.qt6.wrapQtAppsHook ];
          in qtStuff ++ [ pkgs.cmake ];

        mkDepends = { qt5, shell }:
          let
            #decide what QT to use
            qtDependences = if qt5 then
              with pkgs.libsForQt5.qt5; [
                qtbase
                qtsvg
                qttools
                qtmultimedia
              ] ++ [ pkgs.libsForQt5.karchive ]
            else
              with pkgs; [ qt6.qtbase qt6.qtsvg qt6.qttools qt6.qtmultimedia ];

            #Other dependencies required for building
            otherDeps = with pkgs; [
              cmake

              git
              libxkbcommon
              gcc
              ninja
              libzip
              libsodium
              libmicrohttpd
            ];

            shellDpeneds =
              if shell then with pkgs; [ rustc rustfmt ] else with pkgs; [ clippy ];

          in qtDependences ++ shellDpeneds ++ otherDeps;

        mkDpShell = { useQt5, preset, debug ? false }:
          pkgs.mkShell {

            nativeBuildInputs = mkNativeInputs { qt5 = useQt5; };

            buildInputs = mkDepends {
              qt5 = useQt5;
              shell = true;
            };

            shellHook = ''

              export ROOT=$PWD/Nixpile-build/${preset}
              mkdir -p $ROOT

              firstBuild() {
                cmake -S $ROOT/../../ -B $ROOT \
                  --preset ${preset} \
                  -DCMAKE_INSTALL_PREFIX=$out
                cmake --build $ROOT
                wrapQtApp $ROOT/bin/drawpile
              }

              incrementalBuild() {
                cmake --build $ROOT
                wrapQtApp $ROOT/bin/drawpile
              }

              incrementalRun() {
                incrementalBuild
                $ROOT/bin/drawpile
              }

            '';
          };

        mkDrawpile = { useQt5, preset, debug ? false }:
          pkgs.rustPlatform.buildRustPackage {
            name = "drawpile";
            src = self;

            nativeBuildInputs = mkNativeInputs { qt5 = useQt5; };

            buildInputs = mkDepends {
              qt5 = useQt5;
              shell = false;
            };

            cargoLock = { lockFile = ./Cargo.lock; };

            configurePhase = ''
              cmake -S ./ -B Drawpile-build \
                --preset ${preset} \
                -DCMAKE_INSTALL_PREFIX=$out
            '';

            enableParallelBuilding = true;

            buildPhase = ''
              cmake --build ./Drawpile-build
            '';

            installPhase = ''
              mkdir -p $out
              cmake --install ./Drawpile-build
            '';
          };

      in rec {

        packages = rec {
          debug-qt6-all-ninja = mkDrawpile {
            preset = "linux-debug-qt6-all-ninja";
            useQt5 = false;
          };
          release-qt6-all-ninja = mkDrawpile {
            preset = "linux-release-qt6-all-ninja";
            useQt5 = false;
          };
          release-qt6-server-ninja = mkDrawpile {
            preset = "linux-release-qt6-server-ninja";
            useQt5 = false;
          };

          debug-qt5-all-ninja = mkDrawpile {
            preset = "linux-debug-qt5-all-ninja";
            useQt5 = true;
          };

          release-qt5-all-ninja = mkDrawpile {
            preset = "linux-release-qt5-all-ninja";
            useQt5 = true;
          };

          release-qt5-server-ninja = mkDrawpile {
            preset = "linux-release-qt5-server-ninja";
            useQt5 = true;
          };

          default = release-qt6-all-ninja;
        };

        #Dev shells

        devShells = rec {
          debug-qt6-all-ninja = mkDpShell {
            preset = "linux-debug-qt6-all-ninja";
            useQt5 = false;
            debug = true;
          };
          release-qt6-all-ninja = mkDpShell {
            preset = "linux-release-qt6-all-ninja";
            useQt5 = false;
            debug = true;
          };
          release-qt6-server-ninja = mkDpShell {
            preset = "linux-release-qt6-server-ninja";
            useQt5 = false;
            debug = true;
          };

          debug-qt5-all-ninja = mkDpShell {
            preset = "linux-debug-qt5-all-ninja";
            useQt5 = true;
            debug = true;
          };
          release-qt5-all-ninja = mkDpShell {
            preset = "linux-release-qt5-all-ninja";
            useQt5 = true;
            debug = true;
          };
          release-qt5-server-ninja = mkDpShell {
            preset = "linux-release-qt5-server-ninja";
            useQt5 = true;
            debug = true;
          };

          default = debug-qt6-all-ninja;
        };

        formatter = pkgs.nixfmt;
      });
}