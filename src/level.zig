const man = @import("manager.zig");
const std = @import("std");
const gpu = @import("rendering/rendering.zig");

const Function = @import("block.zig").BlockFunctions;
const Block = man.Block;
const Manager = man.Manager;
const Renderer = @import("rendering/renderer.zig").Renderer;

pub const LevelError = error{
    VulkanFailed,
};

pub const Level = struct {
    manager: Manager,
    renderer: Renderer,
    allocator: std.mem.Allocator,

    pub fn deinit(self: *Level) !void {
        try self.manager.deinit();
    }

    pub fn init(gpa: std.mem.Allocator, blocks: []const *Block, functions: []const Function) !@This() {
        var manager: Manager = try .init(gpa, blocks.len);
        var initial_props = try manager.get_properties();
        defer initial_props.deinit(manager.allocator);
        const renderer: Renderer = try .init(initial_props.items);

        for (blocks, functions) |*block, *function| {
            try manager.append(block.*, function.*);
        }

        return .{
            .manager = manager,
            .renderer = renderer,
            .allocator = gpa,
        };
    }

    pub fn update(self: *Level) !void {
        self.manager.update_all();
    }

    pub fn tick(self: *Level) !void {
        var properties = try self.manager.get_properties();
        try self.renderer.change_blocks(try properties.toOwnedSlice(self.manager.allocator));
        try self.manager.update_all(self.renderer.app.getDeltaTime());
        try self.renderer.render();
    }

    pub fn run(self: *Level) !void {
        while (true) {
            try self.tick();
        }
    }
};
