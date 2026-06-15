const base = @import("../block.zig");
const std = @import("std");
const fs = std.fs;
const Alloc = std.heap.FixedBufferAllocator;

pub const TestBlock = struct {
    x: u16,
    y: u16,
    z: u16,

    texture_file: []const u8,

    fn get_texture(self: *const TestBlock, allocator: std.mem.Allocator) []const u8 {
        return fs.cwd().readFileAlloc(allocator, self.texture_file, std.math.maxInt(usize)) catch |err| {
            std.debug.panic("Failed to read texture: {}", .{err});
        };
    }

    fn properties(self: *const anyopaque) base.BlockProperties {
        const block: *const TestBlock = @ptrCast(@alignCast(self));

        return .{
            .position = .{ block.x, block.y, block.z },
            .rotation = .{ 0, 0, 0 },
            .texture = block.texture_file,
        };
    }

    fn update(self: *anyopaque, deltaTime: f32) !void {
        const block: *TestBlock = @ptrCast(@alignCast(self));
        if (block.x % 2 == 0) {
            block.x += 1;
        } else {
            block.x -= 1;
        }
        std.debug.print("delta time: {}\n", .{deltaTime});
        std.debug.print("Frames per second: {}\n", .{1000 / deltaTime});
    }

    fn destroy(self: *anyopaque, allocator: std.mem.Allocator) !void {
        const block: *TestBlock = @ptrCast(@alignCast(self));
        allocator.destroy(block);
        return;
    }

    pub fn functions(_: *const TestBlock) base.BlockFunctions {
        return .{ .properties = &TestBlock.properties, .destroy = &TestBlock.destroy, .update = &TestBlock.update };
    }

    pub fn init(pos: @Vector(3, u16), texture: ?[]const u8) !TestBlock {
        const text = texture.?;
        return .{ .x = pos[0], .y = pos[1], .z = pos[2], .texture_file = text };
    }
};
