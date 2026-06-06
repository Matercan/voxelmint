const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});
    const make_shaders = b.addSystemCommand(&.{ "vulkan-sdk/1.4.350.1/x86_64/bin/slangc", "src/rendering/shader.slang", "-target", "spirv", "-profile", "spirv_1_4", "-emit-spirv-directly", "-fvk-use-entrypoint-name", "-entry", "vertMain", "-entry", "fragMain", "-o", "src/rendering/slang.spv" });
    b.getInstallStep().dependOn(&make_shaders.step);
    const shaders_step = b.step("Compile shaders", "Compiles the shaders");
    shaders_step.dependOn(&make_shaders.step);

    const mod = b.addModule("voxelmint", .{
        .root_source_file = b.path("src/root.zig"),
        .target = target,
    });

    const is_release = optimize != .Debug;

    mod.addCMacro("ENABLE_CPP20_MODuLE", "1");
    if (is_release) {
        mod.addCMacro("NDEBUG", "1");
    }

    mod.addCSourceFiles(.{
        .files = &.{ "src/rendering/vulkan.cpp", "src/rendering/vertex.cpp" },
        .flags = if (is_release)
            &.{ "-Wall", "-std=c++23", "-O2", "-Isrc/include/" }
        else
            &.{ "-Wall", "-std=c++23", "-g", "-Isrc/include/" },
    });

    mod.addIncludePath(b.path("src/rendering/"));
    mod.linkSystemLibrary("vulkan", .{});
    mod.linkSystemLibrary("wayland-client", .{});
    mod.linkSystemLibrary("glfw", .{});
    mod.link_libcpp = true;

    const exe = b.addExecutable(.{
        .name = "voxelmint",
        .root_module = b.createModule(.{
            .root_source_file = b.path("src/main.zig"),
            .target = target,
            .optimize = optimize,
            .imports = &.{
                .{ .name = "voxelmint", .module = mod },
            },
        }),
    });

    b.installArtifact(exe);

    const run_step = b.step("run", "Run the app");
    const run_cmd = b.addRunArtifact(exe);
    run_step.dependOn(&run_cmd.step);
    run_cmd.step.dependOn(b.getInstallStep());

    if (b.args) |args| {
        run_cmd.addArgs(args);
    }

    const mod_tests = b.addTest(.{
        .root_module = mod,
    });

    const run_mod_tests = b.addRunArtifact(mod_tests);
    const exe_tests = b.addTest(.{
        .root_module = exe.root_module,
    });

    const run_exe_tests = b.addRunArtifact(exe_tests);

    const test_step = b.step("test", "Run tests");
    test_step.dependOn(&run_mod_tests.step);
    test_step.dependOn(&run_exe_tests.step);
}
