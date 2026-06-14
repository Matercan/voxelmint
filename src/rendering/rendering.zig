const Vertex = @import("block.zig").Vertex;

pub const POV: f16 = 0.7;
pub extern const WIDTH: u32;
pub extern const HEIGHT: u32;
pub extern const FPS: 120;

pub const Application = opaque {
    pub extern fn initApplication(*Application) callconv(.c) void;
    pub extern fn tickApplication(*Application) callconv(.c) void;
    pub extern fn closeApplication(*Application) callconv(.c) void;
    pub extern fn setVertices(*Application, [*]Vertex, usize, [*]u32, usize) callconv(.c) void;
    pub extern fn pushVertices(*Application, [*]Vertex, usize, [*]u32, usize) callconv(.c) void;
    pub extern fn getTextureIndex(*Application, [*]const u8) u32;
    pub extern fn getDeltaTime(*Application) f32;
};
pub extern fn getApplication() callconv(.c) *Application;
pub extern fn run() callconv(.c) c_int;
