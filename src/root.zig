pub const manager = @import("manager.zig");
pub const block = @import("block.zig");
pub const renderer = @import("rendering/renderer.zig");
pub const level = @import("level.zig");
pub const blocks = @import("blocks/test.zig");
pub const vulkan = @import("rendering/rendering.zig");
pub const tex = @import("rendering/textures.zig");

test "level" {
    const std = @import("std");
    const TestBlock = @import("blocks/test.zig").TestBlock;
    const expect = std.testing.expect;

    var gpa: std.heap.DebugAllocator(.{}) = .init;
    const allocator = gpa.allocator();
    defer {
        const deinit_status = gpa.deinit();

        if (deinit_status == .leak) expect(false) catch @panic("TEST FAIL");
    }

    const blk = try allocator.create(TestBlock);
    blk.* = try TestBlock.init(.{ 0, 0, 0 }, "textures/image.png");
    const func = blk.functions();

    var blks = [_]*manager.Block{@ptrCast(blk)};
    var fns = [_]block.BlockFunctions{func};

    var lvl: level.Level = try .init(allocator, &blks, &fns);
    for (0..50000) |i| {
        std.debug.print("i: {}\n", .{i});
        try lvl.tick();
    }

    try lvl.deinit();
}
