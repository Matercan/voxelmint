const Block = @import("block.zig").BlockProperties;
const std = @import("std");

pub const Renderer = struct {
    current_blocks: []const Block,

    pub fn change_blocks(self: *Renderer, blocks: []const Block) void {
        self.current_blocks = blocks;
    }

    pub fn render(_: *Renderer) !void {
        return;
    }

    pub fn init(blocks: []const Block) Renderer {
        return .{ .current_blocks = blocks };
    }
};
