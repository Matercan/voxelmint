{
  pkgs ? import <nixpkgs> { },
}:

pkgs.mkShell {
  buildInputs = with pkgs; [
    zig
    gcc
    clang
    libclang
    pkg-config

    vulkan-headers
    vulkan-loader
    vulkan-tools
    vulkan-validation-layers
    raylib
    libxcb
    libzip

    glm
    nlohmann_json
    glfw3
    wayland
  ];

  shellHook = /* bash */ ''
    cd ..
  '';

  WAYLAND_PROTOCOLS_DIR = "${pkgs.wayland-protocols}/share/wayland-protocols";
  VULKAN_PROTOCOLS_DIR = "${pkgs.vulkan-headers}";
  VK_LAYER_PATH = "${pkgs.vulkan-validation-layers}/share/vulkan/explicit_layer.d";
}
