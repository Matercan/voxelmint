const std = @import("std");
const voxelmint = @import("voxelmint");
const TestBlock = voxelmint.blocks.TestBlock;

pub fn main() !void {

    
     const arena = try std.heap.page_allocator.create(std.heap.ArenaAllocator);
     arena.* = .init(std.heap.page_allocator);
     const allocator = arena.allocator();

     const block = try allocator.create(TestBlock);
     block.* = try TestBlock.init(.{ 0, 0, 0 }, "textures/image.png");
     const func = block.functions();

     var blks = try allocator.alloc(*voxelmint.manager.Block, 1);
     blks[0] = @ptrCast(block);
     var fns = try allocator.alloc(voxelmint.block.BlockFunctions, 1);
     fns[0] = func;

     var level: voxelmint.level.Level = try .init(allocator, blks, fns);

     while (true) {
        try level.tick();
     }
    
}
