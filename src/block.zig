const std = @import("std");

pub const BlockError = error{
    CannotDestroy,
    NoProperties,
};

/// a struct holding the properties of a block. Each vector3 holds values in this order: x, y, and z.
pub const BlockProperties = struct {
    position: @Vector(3, isize) = .{ 0, 0, 0 },
    rotation: @Vector(3, isize) = .{ 0, 0, 0 },
    texture: []const u8,
};

pub const BlockFunctions = struct { update: *const fn (*anyopaque, f32) anyerror!void, destroy: *const fn (*anyopaque, std.mem.Allocator) anyerror!void, properties: *const fn (*const anyopaque) BlockProperties };
