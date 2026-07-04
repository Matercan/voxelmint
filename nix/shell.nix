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
  glm,
  glfw3,
  vulkan-validation-layers,
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
    glm
    glfw3
  ];

  shellHook = /* bash */ ''
    zsh
  '';

  VULKAN_PROTOCOLS_DIR = "${vulkan-headers}";
  VK_LAYER_PATH = "${vulkan-validation-layers}/share/vulkan/explicit_layer.d";
}
