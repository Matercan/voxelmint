const std = @import("std");
const gpu = @import("rendering.zig");
const block = @import("block.zig");
const Block = @import("../block.zig").BlockProperties;

pub const Renderer = struct {
    current_blocks: []const Block,
    arena: std.heap.ArenaAllocator,
    app: *gpu.Application,

    pub fn init(blocks: []const Block) !Renderer {
        const arena = std.heap.ArenaAllocator.init(std.heap.page_allocator);
        const app = gpu.getApplication();
        app.initApplication();

        return .{
            .current_blocks = blocks,
            .arena = arena,
            .app = app,
        };
    }

    pub fn deinit(self: *Renderer) void {
        self.app.closeApplication();
        self.arena.deinit();
    }

    pub fn change_blocks(self: *Renderer, blocks: []const Block) !void {
        self.current_blocks = blocks;

        _ = self.arena.reset(.retain_capacity);
        const allocator = self.arena.allocator();

        var verticesList = try std.ArrayList(block.Vertex).initCapacity(allocator, blocks.len * 24);
        var indicesList = try std.ArrayList(u32).initCapacity(allocator, blocks.len * 36);

        defer indicesList.deinit(allocator);
        defer verticesList.deinit(allocator);

        for (self.current_blocks, 0..) |blk, i| {
            const vertices = block.convertBlockToVertexes(blk);
            try verticesList.appendSlice(allocator, &vertices);

            const base: u32 = @intCast(i * 24);
            for (block.indices) |idx| {
                try indicesList.append(allocator, base + idx);
            }
        }

        self.app.setVertices(
            verticesList.items.ptr,
            verticesList.items.len,
            indicesList.items.ptr,
            indicesList.items.len,
        );
    }

    pub fn render(_: *Renderer) !void {}
};
