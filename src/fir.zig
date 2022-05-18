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
            else => return FirError.UnhandledEvalType
        };

        // get expr type
        switch (expr.@"type".id) {
            c.ID_SCOPE => {
                const expr_scope = c.AstExpr_scope(expr);
                const scope = Scope{
                    .exprs = cBumpAlloc(*Fir, ctx.pool, expr_scope.len)
                };

                for (expr_scope.exprs[0..expr_scope.len]) |child, i| {
                    scope.exprs[i] = try Fir.fromAstExpr(ctx, child);
                }

                self.data = Data{
                    .scope = scope
                };
            },
            c.ID_LITERAL => {
                const expr_tok = c.AstExpr_tok(expr);
                const tok_end = expr_tok.start + expr_tok.len;
                const slice = ctx.file.text.str[expr_tok.start..tok_end];

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
            else => return FirError.UnhandledAstExprType
        }

        return self;
    }

    fn dumpR(self: Self, level: c_int) void {
        _ = c.printf("%*s", level * 2, "");
        _ = c.printf(c.TC_RED ++ "%s" ++ c.TC_RESET ++ " %s ",
                     @tagName(self.evaltype).ptr,
                     @tagName(self.data).ptr);

        switch (self.data) {
            .scope => |scope| {
                _ = c.printf("\n");

                for (scope.exprs) |child| {
                    child.dumpR(level + 1);
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
            else => unreachable
        }
    }

    fn dump(self: Self) void {
        self.dumpR(0);
    }
};

// c interface =================================================================

export fn gen_fir(
    pool: *c.Bump, file: *c.File, ast: *c.AstExpr
) *const Fir {
    var arena = std.heap.ArenaAllocator.init(std.heap.page_allocator);
    defer arena.deinit();

    const ctx = FirCtx{
        .pool = pool,
        .arena = arena.allocator(),
        .file = file
    };

    return Fir.fromAstExpr(ctx, ast) catch |e| {
        c.fungus_panic("FIR generation failed with error %s.",
                       @errorName(e).ptr);
        unreachable;
    };
}

export fn Fir_dump(fir: *Fir) void {
    fir.dump();
}