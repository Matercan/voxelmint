const Vertex = @import("block.zig").Vertex;

pub const POV: f16 = 0.7;
pub extern const WIDTH: u32;
pub extern const HEIGHT: u32;
pub extern const FPS: 120;

pub const Application = opaque {
    pub extern fn initApplication(*Application) void;
    pub extern fn tickApplication(*Application) void;
    pub extern fn closeApplication(*Application) void;
    pub extern fn setVertices(*Application, [*]Vertex, usize, [*]u32, usize) callconv(.c) void;
};
pub extern fn getApplication() *Application;
pub extern fn run() callconv(.c) c_int;
