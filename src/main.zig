const std = @import("std");
const voxelmint = @import("voxelmint");

pub fn main() !void {
    std.debug.print("All your {s} are belong to us.\n", .{"codebase"});
    try voxelmint.bufferedPrint();
}
