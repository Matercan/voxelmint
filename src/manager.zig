const block = @import("block.zig");
const std = @import("std");
const Allocator = std.heap.ArenaAllocator;
const ArrayList = std.ArrayList;

pub const Block = opaque {};

pub const Manager = struct {
    buffer: ArrayList(block.BlockFunctions),
    blocks: ArrayList(*Block),
    count: usize,
    allocator: std.mem.Allocator,

    pub fn init(gpa: std.mem.Allocator, init_size: usize) !Manager {
        const functions: ArrayList(block.BlockFunctions) = try ArrayList(block.BlockFunctions).initCapacity(gpa, init_size);
        const blocks: ArrayList(*Block) = try ArrayList(*Block).initCapacity(gpa, init_size);

        return .{
            .buffer = functions,
            .blocks = blocks,
            .count = init_size,
            .allocator = gpa,
        };
    }

    pub fn deinit(self: *Manager) !void {
        for (self.buffer.items, self.blocks.items) |func, blk| {
            try func.destroy(blk, self.allocator);
        }
        self.buffer.deinit(self.allocator);
        self.blocks.deinit(self.allocator);
    }

    pub fn update_all(self: *Manager) !void {
        for (self.buffer.items, self.blocks.items) |blk, blk_ptr| {
            try blk.update(blk_ptr);
        }
    }

    pub fn get_properties(self: *const Manager) !ArrayList(block.BlockProperties) {
        var properties: ArrayList(block.BlockProperties) = .empty;
        errdefer properties.deinit(self.allocator);

        for (self.buffer.items, self.blocks.items) |blk, item| {
            std.debug.print("block ptr: {*}\n", .{item});

            std.debug.print("Finaly hope!", .{});
            const prop = blk.properties(item);
            try properties.append(self.allocator, prop);
        }

        return properties;
    }

    pub fn append(self: *Manager, ptr: *Block, functions: block.BlockFunctions) !void {
        try self.buffer.append(self.allocator, functions);
        try self.blocks.append(self.allocator, ptr);
        if (self.buffer.items.len > self.count) {
            self.count = self.buffer.items.len;
        }
    }
};
