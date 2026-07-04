{
  mkShell,
  rustc,
  cargo,
  rust-analyzer,
  rustfmt,
  clippy,
  pkg-config,
  gcc,
  libclang,
  libzip,
  wayland,
  vulkan-headers,
  vulkan-tools,
  vulkan-loader,
  vulkan-validation-layers,
  shader-slang,
  glm,
  glfw3,
}:

mkShell {
  name = "voxelmint-dev";
  strictDeps = true;

  nativeBuildInputs = [
    libclang
    cargo
    rustc
    clippy
    rustfmt
    rust-analyzer
    pkg-config
    gcc
  ];

  buildInputs = [
    vulkan-headers
    vulkan-loader
    vulkan-tools
    vulkan-validation-layers
    libzip
    wayland
    shader-slang
    glm
    glfw3
  ];

  shellHook = /* bash */ ''
    zsh
  '';

  VULKAN_PROTOCOLS_DIR = "${vulkan-headers}";
  VK_LAYER_PATH = "${vulkan-validation-layers}/share/vulkan/explicit_layer.d";
}
