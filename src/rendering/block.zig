const std = @import("std");
const block = @import("../block.zig");
const Block = @import("../manager.zig").Block;
const gpu = @import("rendering.zig");

pub const indices = [_]u32{
    0,  1,  2,  2,  3,  0,
    4,  5,  6,  6,  7,  4,
    8,  9,  10, 10, 11, 8,
    12, 13, 14, 14, 15, 12,
    16, 17, 18, 18, 19, 16,
    20, 21, 22, 22, 23, 20,
};

const blockEdges: [8][3]u8 = .{
    .{ 0, 0, 0 },
    .{ 0, 0, 1 },
    .{ 0, 1, 0 },
    .{ 0, 1, 1 },
    .{ 1, 0, 0 },
    .{ 1, 0, 1 },
    .{ 1, 1, 0 },
    .{ 1, 1, 1 },
};
const faceVertices = [_][4]u8{
    .{ 0, 1, 5, 4 }, // bottom (-Y)
    .{ 2, 6, 7, 3 }, // top (+Y)
    .{ 0, 4, 6, 2 }, // front (-Z)
    .{ 1, 3, 7, 5 }, // back (+Z)
    .{ 0, 2, 3, 1 }, // left (-X)
    .{ 4, 5, 7, 6 }, // right (+X)
};
const faceTexEdges = [6][4][2]f32{
    .{ .{ 0, 0 }, .{ 1, 0 }, .{ 1, 1 }, .{ 0, 1 } }, // bottom (-Y)
    .{ .{ 0, 1 }, .{ 1, 1 }, .{ 1, 0 }, .{ 0, 0 } }, // top (+Y)
    .{ .{ 0, 1 }, .{ 1, 1 }, .{ 1, 0 }, .{ 0, 0 } }, // front (-Z)
    .{ .{ 1, 1 }, .{ 1, 0 }, .{ 0, 0 }, .{ 0, 1 } }, // back (+Z)
    .{ .{ 1, 1 }, .{ 1, 0 }, .{ 0, 0 }, .{ 0, 1 } }, // left (-X)
    .{ .{ 0, 1 }, .{ 1, 1 }, .{ 1, 0 }, .{ 0, 0 } }, // right (+X)
};

/// Model cube: 
///
///                  y+
///                  ↑
///
///         3 ----------- 7
///        /|            /|
///       / |           / |
///      /  |          /  |
///     2 ----------- 6   |
///     |   |         |   |
///     |   |         |   |
///     |   1 --------|---5   → z+
///     |  /          |  /
///     | /           | /
///     |/            |/
///     0 ----------- 4
///    /
///   /
///  x+
///

pub const Vertex = extern struct {
    pos: [3]f32,
    color: [3]f32,
    tex_coord: [2]f32,
};

pub fn convertBlockToVertexes(blk: block.BlockProperties) [24]Vertex {
    const pos = blk.position;
    var out: [24]Vertex = undefined;

    var f: usize = 0;
    while (f < 6) : (f += 1) {
        var v: usize = 0;
        while (v < 4) : (v += 1) {
            const idx = faceVertices[f][v];

            const x = @as(f32, @floatFromInt(pos[0] + blockEdges[idx][0]));
            const y = @as(f32, @floatFromInt(pos[1] + blockEdges[idx][1]));
            const z = @as(f32, @floatFromInt(pos[2] + blockEdges[idx][2]));

            const vertexIndex = f * 4 + v;

            out[vertexIndex] = .{
                .pos = .{ x, y, z },
                .color = if (f % 2 == 0) .{ 1.0, 0.0, 1.0 } else .{ 0.0, 0.0, 0.0 },
                .tex_coord = faceTexEdges[f][v],
            };
        }
    }

    return out;
}
