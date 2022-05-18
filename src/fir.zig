const std = @import("std");
const Allocator = std.mem.Allocator;

const c = @cImport({
    @cInclude("fir.h");
    @cInclude("fungus.h");
});

fn cBumpCreate(comptime T: type, pool: *c.Bump) *T {
    const mem = c.Bump_alloc(pool, @sizeOf(T));

    return @ptrCast(*T, @alignCast(@alignOf(T), mem));
}

fn cBumpAlloc(comptime T: type, pool: *c.Bump, n: usize) []T {
    const mem = c.Bump_alloc(pool, n * @sizeOf(T));

    return @ptrCast([*]T, @alignCast(@alignOf(T), mem))[0..n];
}

const FirCtx = struct {
    pool: *c.Bump,
    arena: Allocator,
    file: *c.File
};

pub const FirError = error {
    UnhandledEvalType,
    UnhandledAstExprType,
    UnhandledLiteralType,
    InvalidLiteral,
};

fn sliceOfAstExpr(file: *c.File, expr: *c.AstExpr) []const u8 {
    const expr_tok = c.AstExpr_tok(expr);
    return file.text.str[expr_tok.start..expr_tok.start + expr_tok.len];
}

pub const Fir = struct {
    evaltype: Type,
    data: Data,

    pub const Type = enum {
        I64,
        F64,
        Bool,
    };

    pub const Data = union(enum) {
        scope: Scope,
        ident: Ident,
        lit: Literal,
        bin_op: BinOp,
    };

    pub const Scope = struct {
        exprs: []*Fir,
    };

    pub const Ident = struct {
        str: []const u8,
    };

    pub const Literal = union(enum) {
        int: u64,
        float: f64,
        boolean: bool,
    };

    pub const BinOp = struct {
        kind: enum {
            Add,
            Sub,
            Mul,
            Div,
            Mod,
        },
        lhs: *Fir,
        rhs: *Fir
    };

    const Self = @This();

    fn fromAstExpr(ctx: FirCtx, expr: *c.AstExpr) FirError!*Self {
        var self = cBumpCreate(Self, ctx.pool);

        // get evaltype
        self.evaltype = switch (expr.evaltype.id) {
            c.ID_INT => .I64,
            c.ID_FLOAT => .F64,
            c.ID_BOOL => .Bool,
            else => {
                c.AstExpr_error(ctx.file, expr, "unknown eval type.");
                return FirError.UnhandledEvalType;
            }
        };

        // get expr type
        switch (expr.@"type".id) {
            c.ID_SCOPE => {
                const rule = c.AstExpr_rule(expr);
                const scope = Scope{
                    .exprs = cBumpAlloc(*Fir, ctx.pool, rule.len)
                };

                for (rule.exprs[0..rule.len]) |child, i| {
                    scope.exprs[i] = try Fir.fromAstExpr(ctx, child);
                }

                self.data = Data{
                    .scope = scope
                };
            },
            c.ID_LITERAL => {
                const slice = sliceOfAstExpr(ctx.file, expr);

                errdefer {
                    c.AstExpr_error(ctx.file, expr, "could not parse literal.");
                }

                self.data = Data{
                    .lit = switch (expr.evaltype.id) {
                        c.ID_INT => Literal{
                            .int = std.fmt.parseInt(u64, slice, 0)
                                catch return FirError.InvalidLiteral
                        },
                        c.ID_FLOAT => Literal{
                            .float = std.fmt.parseFloat(f64, slice)
                                catch return FirError.InvalidLiteral
                        },
                        c.ID_BOOL => Literal{
                            .boolean = slice[0] == 't'
                        },
                        else => return FirError.UnhandledLiteralType
                    }
                };
            },
            c.ID_ADD, c.ID_SUBTRACT, c.ID_MULTIPLY, c.ID_DIVIDE, c.ID_MODULO
                => |id| {
                // binary operators
                const rule = c.AstExpr_rule(expr);
                const bin_op = BinOp{
                    .kind = switch (id) {
                        c.ID_ADD => .Add,
                        c.ID_SUBTRACT => .Sub,
                        c.ID_MULTIPLY => .Mul,
                        c.ID_DIVIDE => .Div,
                        c.ID_MODULO => .Mod,
                        else => unreachable
                    },
                    .lhs = try Fir.fromAstExpr(ctx, rule.exprs[0]),
                    .rhs = try Fir.fromAstExpr(ctx, rule.exprs[2])
                };

                self.data = Data{
                    .bin_op = bin_op
                };
            },
            else => {
                const name = @ptrCast(*const c.View, c.Type_name(expr.@"type"));

                c.AstExpr_error(ctx.file, expr, "unhandled expr type `%.*s`.",
                                @intCast(c_int, name.len), name.str);

                return FirError.UnhandledAstExprType;
            }
        }

        return self;
    }

    fn dumpR(self: Self, level: c_int) void {
        _ = c.printf("%*s", level * 2, "");
        _ = c.printf(c.TC_RED ++ "%s" ++ c.TC_RESET ++ " %s ",
                     @tagName(self.evaltype).ptr,
                     @tagName(self.data).ptr);

        const next_level = level + 1;

        switch (self.data) {
            .scope => |scope| {
                _ = c.printf("\n");

                for (scope.exprs) |child| {
                    child.dumpR(next_level);
                }
            },
            .lit => |lit| {
                _ = c.printf(c.TC_MAGENTA);

                switch (lit) {
                    .int => |value|
                        _ = c.printf("%jd", @intCast(c.uintmax_t, value)),
                    .float => |value| _ = c.printf("%f\n", value),
                    .boolean => |value| {
                        const str = if (value) "true" else "false";
                        _ = c.printf("%s", str.ptr);
                    }
                }

                _ = c.printf(c.TC_RESET ++ "\n");
            },
            .bin_op => |bin_op| {
                const name = @tagName(bin_op.kind);
                var buf: [32]u8 = undefined;
                _ = std.ascii.lowerString(buf[0..], name);
                buf[name.len] = 0;

                _ = c.printf("%s\n", buf);

                bin_op.lhs.dumpR(next_level);
                bin_op.rhs.dumpR(next_level);
            },
            else => |tag| {
                _ = c.printf("unhandled fir type %s in Fir.dumpR().\n",
                             @tagName(tag).ptr);
            }
        }
    }

    fn dump(self: Self) void {
        self.dumpR(0);
    }
};

// c interface =================================================================

export fn gen_fir(
    pool: *c.Bump, file: *c.File, ast: *c.AstExpr
) ?*const Fir {
    var arena = std.heap.ArenaAllocator.init(std.heap.page_allocator);
    defer arena.deinit();

    const ctx = FirCtx{
        .pool = pool,
        .arena = arena.allocator(),
        .file = file
    };

    return Fir.fromAstExpr(ctx, ast) catch on_err: {
        c.global_error = true;
        break :on_err null;
    };
}

export fn Fir_dump(fir: *Fir) void {
    fir.dump();
}