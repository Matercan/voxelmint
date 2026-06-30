{
  lib,
  rustPlatform,
  pkg-config,
  wayland,
  libzip,
  vulkan-headers,
  glfw
}: let 
  cargoTOML = lib.importTOML ../Cargo.toml;
in 
  rustPlatform.buildRustPackage (finalAttrs: {
    pname = "voxelmint";
    version = cargoTOML.workspace.package.version;

    src = let
      fs = lib.fileset;
      s = ../.;
    in 
      fs.toSource {
        root = s;
        fileset = fs.unions [
          (s + /src)
          (s + /Cargo.lock)
          (s + /Cargo.toml)
        ];
      };

    cargoLock.lockFile = "${finalAttrs.src}/Cargo.lock";
    enableParallelBuilding = true;

    strictDeps = true;
    nativeBuildInputs = [
      pkg-config
    ];

    buildInputs = [
      libzip
      wayland
      glfw
      vulkan-headers
    ];

    meta = {
      description = "A fast, multithreaded, vulkan game engine";
      license = lib.licenses.gnug3;
      maintainers = with lib.maintainers; [Matercan];
      mainProgram = "voxelmint";
      platforms = lib.platfrosm.linux;
    };
  })
