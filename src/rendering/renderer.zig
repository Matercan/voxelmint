const std = @import("std");
const gpu = @import("rendering.zig");
const block = @import("block.zig");
const Block = @import("../block.zig").BlockProperties;

pub const Renderer = struct {
    current_blocks: []const Block,
    arena: std.heap.ArenaAllocator,
    app: *gpu.Application,
    mutex: std.Thread.Mutex,

    pub fn init(blocks: []const Block) !Renderer {
        const arena = std.heap.ArenaAllocator.init(std.heap.page_allocator);
        const app = gpu.getApplication();
        app.initApplication();

        return .{
            .current_blocks = blocks,
            .arena = arena,
            .app = app,
            .mutex = std.Thread.Mutex{},
        };
    }

    pub fn deinit(self: *Renderer) void {
        self.app.closeApplication();
        self.arena.deinit();
    }

    pub fn render(self: *Renderer) !void {
        _ = self.arena.reset(.retain_capacity);
        const allocator = self.arena.allocator();

        // copy current_blocks under the lock, then release
        self.mutex.lock();
        const blocks = self.current_blocks;
        self.mutex.unlock();

        var verticesList = try std.ArrayList(block.Vertex).initCapacity(allocator, blocks.len * 24);
        defer verticesList.deinit(allocator);

        var indicesList = try std.ArrayList(u32).initCapacity(allocator, blocks.len * 36);
        defer indicesList.deinit(allocator);

        for (blocks, 0..) |blk, i| {
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

        self.app.tickApplication();
    }

    pub fn change_blocks(self: *Renderer, blocks: []const Block) !void {
        self.mutex.lock();
        self.current_blocks = blocks;
        self.mutex.unlock();
    }
};
