const std = @import("std");

pub const BlockError = error{
    CannotDestroy,
    NoProperties,
};

/// a struct holding the properties of a block. Each vector3 holds values in this order: x, y, and z.
pub const BlockProperties = struct {
    position: @Vector(3, isize) = .{ 0, 0, 0 },
    /// These rotation vectors are stored as angles (in radians) for how much each block is rotated by in each axis,
    /// thus the new point of each pixel can be found by inputting these numbers into sine and cosine within a rotation matrix,
    rotation: @Vector(3, isize) = .{ 0, 0, 0 },
    /// The texture of the block, similar to minecraft's texturing it is a simple square net.
    texture: []const u8,
};

pub const BlockFunctions = struct { update: *const fn (*anyopaque) anyerror!void, destroy: *const fn (*anyopaque, std.mem.Allocator) anyerror!void, properties: *const fn (*const anyopaque) BlockProperties };
