const std = @import("std");
const voxelmint = @import("voxelmint");
const TestBlock = voxelmint.blocks.TestBlock;
pub const tex = voxelmint.tex;

pub fn main() !void {
    _ = tex.getTexManager();

    const arena = try std.heap.page_allocator.create(std.heap.ArenaAllocator);
    arena.* = .init(std.heap.page_allocator);
    const allocator = arena.allocator();

    const dirt_block = try allocator.create(TestBlock);
    dirt_block.* = try TestBlock.init(.{ 0, 0, 0 }, "dirt.gtex");

    const grass_block = try allocator.create(TestBlock);
    grass_block.* = try TestBlock.init(.{ 0, 1, 0 }, "grass.gtex");

    var blks = try allocator.alloc(*voxelmint.manager.Block, 2);
    blks[0] = @ptrCast(dirt_block);
    blks[1] = @ptrCast(grass_block);

    var fns = try allocator.alloc(voxelmint.block.BlockFunctions, 2);
    fns[0] = dirt_block.functions();
    fns[1] = grass_block.functions();

    var level: voxelmint.level.Level = try .init(allocator, blks, fns);

    for (0..100000) |_| {
        try level.tick();
    }
}

