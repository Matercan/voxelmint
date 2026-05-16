const std = @import("std");
const voxelmint = @import("voxelmint");
const TestBlock = voxelmint.blocks.TestBlock;
const wayland = voxelmint.display;

pub fn main() !void {
    const arena = try std.heap.page_allocator.create(std.heap.ArenaAllocator);
    arena.* = .init(std.heap.page_allocator);
    const allocator = arena.allocator();

    const block = try allocator.create(TestBlock);
    block.* = try TestBlock.init(.{ 0, 0, 0 }, "image.png");
    const func = block.functions();

    var blks = try allocator.alloc(*voxelmint.manager.Block, 1);
    blks[0] = @ptrCast(block);
    var fns = try allocator.alloc(voxelmint.block.BlockFunctions, 1);
    fns[0] = func;

    std.debug.print("Blocks ptr: {*}\nFuncs ptr: {*}\n", .{ blks, fns });

    var level: voxelmint.level.Level = try .init(allocator, blks, fns);
    _ = wayland.create_display();
    _ = wayland.dispatch_display(wayland.get_state());
    _ = wayland.clear(wayland.get_state());

    _ = wayland.dispatch_display(wayland.get_state());

    while (true) {
        try level.tick();
        if (wayland.dispatch_display(wayland.get_state()) != 0) {
            std.debug.print("Error", .{});
            break;
        }
    }
}
