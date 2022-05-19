const std = @import("std");

// submit an enum type, returns an array of strings with names in lowercase
// names include a nul terminator
pub fn lowerCaseEnumTags(comptime T: type) [][]u8 {
    comptime {
        const info = @typeInfo(T);
        const fields = info.Enum.fields;

        // find longest field
        var longest_field: usize = 0;
        for (fields) |field| {
            if (field.name.len > longest_field) {
                longest_field = field.name.len;
            }
        }

        // write field names to a buffer
        var buf: [fields.len][longest_field + 1]u8 = undefined;
        var output: [buf.len][]u8 = undefined;

        for (fields) |field, i| {
            std.mem.copy(u8, buf[i][0..], field.name);
            buf[i][field.name.len] = 0;
            output[i] = buf[i][0..field.name.len];

            for (output[i]) |ch, j| {
                if (ch >= 'A' and ch <= 'Z') {
                    output[i][j] = ch + 'a' - 'A';
                }
            }
        }

        return output[0..];
    }
}