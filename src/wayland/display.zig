const State = opaque {};
extern var wl_state: *State;

pub extern fn dispatch_display(*State) callconv(.c) c_int;
pub extern fn create_display() callconv(.c) c_char;
pub extern fn draw_state(*State, []u8, c_long) callconv(.c) c_int;
pub extern fn clear(*State) callconv(.c) c_int;

pub fn get_state() *State {
    return wl_state;
}

test "window" {
    const expect = @import("std").testing.expect;
    var res = 0;
    res = create_display();
    expect(res == 0);
    res = dispatch_display(get_state());
    expect(res == 0);
    res = clear(get_state());
    expect(res == 0);
}
