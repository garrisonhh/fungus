const std = @import("std");
const ArrayList = std.ArrayList;
const Allocator = std.mem.Allocator;
const Fir = @import("fir.zig").Fir;

//! SSA is the context for SSA representation building

const Self = @This();

fns: ArrayList(Function),

/// finalized representation of an SSA function
pub const Function = struct {
};

/// in-progress SSA function
const FnBuilder = struct {

};

fn init(allocator: Allocator) Self {
    return Self{
        .fns = ArrayList(SsaFn).init(allocator),
    };
}

fn deinit(self: Self) void {
    self.fns.deinit();
}