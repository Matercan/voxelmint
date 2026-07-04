{
  lib,
  rustPlatform,
  pkg-config,
  makeWrapper,
  wayland,
  libzip,
  glm,
  vulkan-headers,
  glfw,
  shader-slang,
  vulkan-loader,
  vulkan-validation-layers,
}:
let
  cargoTOML = lib.importTOML ../Cargo.toml;
in
rustPlatform.buildRustPackage (finalAttrs: {
  pname = "voxelmint";
  version = cargoTOML.package.version;

  src =
    let
      fs = lib.fileset;
      s = ../.;
    in
    fs.toSource {
      root = s;
      fileset = fs.unions [
        (s + /src)
        (s + /Cargo.lock)
        (s + /Cargo.toml)
        (s + /build.rs)
      ];
    };

  cargoLock.lockFile = "${finalAttrs.src}/Cargo.lock";
  enableParallelBuilding = true;

  strictDeps = true;
  nativeBuildInputs = [
    pkg-config
    shader-slang
    makeWrapper
  ];

  buildInputs = [
    libzip
    wayland
    glm
    glfw
    vulkan-headers
    vulkan-loader
    vulkan-validation-layers
  ];

  postFixup = ''
    wrapProgram $out/bin/voxelmint \
      --prefix LD_LIBRARY_PATH : ${
        lib.makeLibraryPath [
          vulkan-loader
          wayland
          glfw
        ]
      }
  '';

  meta = {
    description = "A fast, multithreaded, vulkan game engine";
    license = lib.licenses.gpl3;
    maintainers = with lib.maintainers; [ Matercan ];
    mainProgram = "voxelmint";
    platforms = lib.platforms.linux;
  };
})
