const std = @import("std");

pub const zip_stat = extern struct {
    valid: u64,
    name: ?[*:0]const u8,
    index: u64,
    size: u64,
    comp_size: u64,
    mtime: i64,
    crc: u32,
    comp_method: u16,
    encryption_method: u16,
    flags: u32,
};

const zip_file_t = opaque {};

const Zip = opaque {
    extern fn zip_get_num_entries(*Zip, c_int) callconv(.c) i64;
    extern fn zip_stat_index(*Zip, u64, c_int, *zip_stat) callconv(.c) c_int;
    extern fn zip_close(*Zip) callconv(.c) c_int;
    extern fn zip_fopen_index(*Zip, u64, c_int) *zip_file_t;
    extern fn zip_fclose(*zip_file_t) c_int;
    extern fn zip_fread(*zip_file_t, [*]u8, u64) u64;

    fn zip_read_entry(z: *Zip, idx: i64, out_len: *usize) callconv(.c) ?[*]u8 {
        var st: zip_stat = undefined;
        _ = zip_stat_index(z, @intCast(idx), 0, &st);

        const zf = zip_fopen_index(z, @intCast(idx), 0);

        const buf = std.heap.c_allocator.alloc(u8, st.size) catch return null;
        const nread: u64 = zip_fread(zf, buf.ptr, st.size);

        _ = zip_fclose(zf);
        if (nread != st.size) {
            std.heap.c_allocator.free(buf);
            return null;
        }
        out_len.* = st.size;
        return buf.ptr;
    }
};
extern fn zip_open([*:0]const u8, c_int, *c_int) callconv(.c) ?*Zip;

extern fn stbi_image_free([*]u8) void;
extern fn stbi_load_from_memory([*]u8, c_int, *c_int, *c_int, *c_int, c_int) [*]u8;
extern fn stbir_resize_uint8_linear([*]const u8, c_int, c_int, c_int, [*]u8, c_int, c_int, c_int, c_int) void;

pub const FaceTexture = extern struct {
    data: [*]const u8,
    label: [*]u8,
    baseTexture: [*:0]const u8,
    texIdx: u32,
    width: u16,
    height: u16,
};

pub const TextureManager = struct {
    zip: *Zip,
    zipIdx: u32,
    texIdx: u32,

    pub fn init() TextureManager {
        var zerr: c_int = 0;

        // 16 means read only.
        const zip = zip_open("textures/out.zip", 16, &zerr);
        if (zip == null) {}
        return .{ .zip = zip.?, .zipIdx = 0, .texIdx = 0 };
    }

    pub fn deinit(self: *TextureManager) void {
        _ = self.zip.zip_close();
        std.heap.c_allocator.destroy(self);
    }

    pub fn textureSize(self: *TextureManager) [2]u8 {
        const pos: usize = 0;
        var data_size: usize = undefined;
        const data = self.zip.zip_read_entry(0, &data_size) orelse return .{ 0, 0 };

        const dim_end = std.mem.indexOfScalarPos(u8, data[0..data_size], pos, 0) orelse return .{ 0, 0 };
        const dimensions = data[pos..dim_end];
        const xIdx = std.mem.indexOf(u8, dimensions, "x") orelse return .{ 0, 0 };
        const width = std.fmt.parseInt(u8, dimensions[0..xIdx], 10) catch 0;
        const height = std.fmt.parseInt(u8, dimensions[xIdx + 1 ..], 10) catch 0;
        return .{ width, height };
    }

    pub fn texturesSize(self: *TextureManager) usize {
        var currentCount: usize = 0;
        const total: usize = @intCast(self.textureCount());
        for (0..total) |_| {
            var pos: usize = 0;
            var data_size: usize = undefined;
            const data = self.zip.zip_read_entry(self.zipIdx, &data_size) orelse break;

            while (pos < data_size) {
                const dim_end = std.mem.indexOfScalarPos(u8, data[0..data_size], pos, 0) orelse return 0;
                const dimensions = data[pos..dim_end];
                const xIdx = std.mem.indexOf(u8, dimensions, "x") orelse return 0;
                const width = std.fmt.parseInt(u16, dimensions[0..xIdx], 10) catch 0;
                const height = std.fmt.parseInt(u16, dimensions[xIdx + 1 ..], 10) catch 0;

                pos = dim_end + 1;
                currentCount += width * height;

                const footer = "\x00end\x00";
                const png_end = std.mem.indexOfPos(u8, data[0..data_size], pos, footer) orelse break;
                pos = png_end + footer.len;
            }
        }

        return currentCount * 4;
    }

    pub fn faceCount(self: *TextureManager) usize {
        var currentCount: usize = 0;
        const total: usize = @intCast(self.textureCount());

        for (0..total) |count| {
            var pos: usize = 0;
            var data_size: usize = undefined;
            const data = self.zip.zip_read_entry(@intCast(count), &data_size) orelse break;

            while (pos < data_size) {
                const footer = "\x00end\x00";
                const png_end = std.mem.indexOfPos(u8, data[0..data_size], pos, footer) orelse break;
                pos = png_end + footer.len;
                currentCount += 1;
            }
        }

        return currentCount;
    }

    pub fn textureCount(self: *TextureManager) i64 {
        return self.zip.zip_get_num_entries(0);
    }

    pub fn getNextTextures(self: *TextureManager, allocator: std.mem.Allocator) ![]const FaceTexture {
        var textures = std.ArrayList(FaceTexture).empty;
        errdefer textures.deinit(allocator);

        var data_size: usize = undefined;
        const d = self.zip.zip_read_entry(self.zipIdx, &data_size).?;
        const data = d[0..data_size];
        var pos: usize = 0;

        var st: zip_stat = undefined;
        _ = self.zip.zip_stat_index(self.zipIdx, 0, &st);
        const name = st.name.?;

        while (pos < data_size) {

            // read "<WxH>\0"
            const dim_end = std.mem.indexOfScalarPos(u8, data[0..data_size], pos, 0) orelse break;
            const dimensions = data[pos..dim_end];
            const xIdx = std.mem.indexOf(u8, dimensions, "x") orelse break;
            var width = std.fmt.parseInt(u8, dimensions[0..xIdx], 10) catch 0;
            var height = std.fmt.parseInt(u8, dimensions[xIdx + 1 ..], 10) catch 0;
            pos = dim_end + 1;

            // read "<face_label>\0"
            const label_end = std.mem.indexOfScalarPos(u8, data, pos, 0) orelse break;
            const face_label = allocator.dupe(u8, data[pos..label_end]) catch break;
            pos = label_end + 1;

            // read PNG bytes until "\0end\0"
            const footer = "\x00end\x00";
            const png_end = std.mem.indexOfPos(u8, data, pos, footer) orelse break;
            var w: c_int = undefined;
            var h: c_int = undefined;
            var ch: c_int = undefined;
            const png_bytes = stbi_load_from_memory(data[pos..png_end].ptr, @intCast(png_end - pos), &w, &h, &ch, 4);

            if (width != w or height != h) {
                width = @intCast(w);
                height = @intCast(h);
            }

            // std.debug.print("Width: {}, Height: {}\n", .{width, height});

            //          const sizes = self.textureSize();
            //          const targetW: u16 = @intCast(sizes[0]);
            //          const targetH: u16 = @intCast(sizes[1]);
            //          if (w != targetW or h != targetH) {
            //              const alloc = allocator.alloc(u8, targetW * targetH * 4) catch break;
            //              const resized: [*]u8 = alloc.ptr;
            //              stbir_resize_uint8_linear(png_bytes, w, h, 4, resized, targetW, targetH, 4, 4);
            //              stbi_image_free(png_bytes);
            //              png_bytes = resized;
            //              width = @intCast(targetW);
            //              height = @intCast(targetH);
            //          }

            const face: FaceTexture = .{ .width = width, .height = height, .label = face_label.ptr, .baseTexture = name, .data = png_bytes, .texIdx = self.texIdx };
            try textures.append(allocator, face);

            pos = png_end + footer.len;
            self.texIdx += 1;
        }

        self.zipIdx += 1;
        allocator.free(data);
        return textures.toOwnedSlice(allocator);
    }
};

pub export fn getTexManager() callconv(.c) ?*TextureManager {
    const manager = std.heap.c_allocator.create(TextureManager) catch return null;
    manager.* = .init();

    return manager;
}
pub export fn freeTexManager(textures: *TextureManager) callconv(.c) void {
    textures.deinit();
}
pub export fn getTextureCount(textures: *TextureManager) callconv(.c) i64 {
    return textures.textureCount();
}
pub export fn getFaceCount(textures: *TextureManager) callconv(.c) usize {
    return textures.faceCount();
}
pub export fn getTexturesSize(textures: *TextureManager) callconv(.c) usize {
    return textures.texturesSize();
}
pub export fn getTextureSize(textures: *TextureManager) callconv(.c) ?[*]const u8 {
    const ptr = std.heap.c_allocator.alloc(u8, 2) catch return null;
    const size = textures.textureSize();
    ptr[0] = size[0];
    ptr[1] = size[1];

    return ptr.ptr;
}
pub export fn getNextTextures(textures: *TextureManager, len: *u8) callconv(.c) ?[*]const FaceTexture {
    const faces: ?[]const FaceTexture = textures.getNextTextures(std.heap.c_allocator) catch null;
    len.* = if (faces) |*value|
        @intCast(value.len)
    else
        0;

    const facesPtr: ?[*]const FaceTexture = if (faces) |*tex|
        tex.ptr
    else
        null;

    return facesPtr;
}
