use std::env;
use std::process::Command;

fn main() {
    let is_release = env::var("PROFILE").unwrap_or_default() == "release";

    let shader_status = Command::new("slangc")
        .args([
            "src/rendering/shader.slang",
            "-target",
            "spirv",
            "-profile",
            "spirv_1_4",
            "-emit-spirv-directly",
            "-fvk-use-entrypoint-name",
            "-entry",
            "vertMain",
            "-entry",
            "fragMain",
            "-o",
            "src/rendering/slang.spv",
        ])
        .status()
        .expect("Failed to run slangc — is the Vulkan SDK in place?");

    assert!(shader_status.success(), "Shader compilation failed");

    println!("cargo:rerun-if-changed=src/rendering/shader.slang");
    println!("cargo:rerun-if-changed=src/rendering/vulkan.cpp");
    println!("cargo:rerun-if-changed=src/rendering/vertex.cpp");
    println!("cargo:rerun-if-changed=src/include/");

    let mut build = cc::Build::new();
    build
        .cpp(true)
        .std("c++23")
        .include("src/include/")
        .include("src/rendering/")
        .file("src/rendering/vulkan.cpp")
        .file("src/rendering/vertex.cpp")
        .define("ENABLE_CPP20_MODULE", "1")
        .flag("-Wall")
        .opt_level(1);

    if is_release {
        build.define("NDEBUG", "1").opt_level(2);
    } else {
        build.flag("-g");
    }

    build.compile("voxelmint_cpp");

    println!("cargo:rustc-link-lib=vulkan");
    println!("cargo:rustc-link-lib=wayland-client");
    println!("cargo:rustc-link-lib=glfw");
    println!("cargo:rustc-link-lib=zip");
    println!("cargo:rustc-link-lib=stdc++");
}
