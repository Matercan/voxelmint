pub const manager = @import("manager.zig");
pub const block = @import("block.zig");
pub const renderer = @import("renderer.zig");
pub const level = @import("level.zig");
pub const blocks = @import("blocks/test.zig");
pub const display = @import("wayland/display.zig");

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
    blk.* = try TestBlock.init(.{ 0, 0, 0 }, "image.png");
    const func = blk.functions();

    std.debug.print("Suck it weirdo\n", .{});

    var blks = [_]*manager.Block{@ptrCast(blk)};
    var fns = [_]block.BlockFunctions{func};

    var lvl: level.Level = try .init(allocator, &blks, &fns);
    for (0..21) |_|{
        std.debug.print("Tick\n", .{});
        try lvl.tick();
    }

    try lvl.deinit();
}
