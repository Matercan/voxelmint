const man = @import("manager.zig");
const std = @import("std");

const Function = @import("block.zig").BlockFunctions;
const Block = man.Block;
const Manager = man.Manager;
const Renderer = @import("renderer.zig").Renderer;

pub const Level = struct {
    manager: Manager,
    renderer: Renderer,
    allocator: std.mem.Allocator,

    pub fn deinit(self: *Level) !void {
        try self.manager.deinit();
    }

    pub fn init(gpa: std.mem.Allocator, blocks: []const *Block, functions: []const Function) !@This() {
        std.debug.print("Blocks ptr: {*}\n Functions ptr: {*}\n", .{blocks, functions});

        var manager: Manager = try .init(gpa, blocks.len);
        var initial_props = try manager.get_properties();
        defer initial_props.deinit(gpa);
        const renderer: Renderer = .init(initial_props.items);

        for (blocks, functions) |*block, *function| {
            std.debug.print("block ptr: {*}\n", .{block});
            std.debug.print("functions ptr: {*}\n", .{function});
            std.debug.print("update: {*}, destroy: {*}, properties: {*}\n", .{function.update, function.destroy, function.properties});
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
        self.renderer.change_blocks(properties.items);
        var thread = try std.Thread.spawn(.{}, Renderer.render, .{&self.renderer});
        try self.manager.update_all();
        thread.join();
        properties.deinit(self.allocator);
    }

    pub fn run(self: *Level) !void {
        while (true) {
            try self.tick();
        }
    }
};
