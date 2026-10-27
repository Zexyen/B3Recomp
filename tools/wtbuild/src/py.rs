//! Python's own spellings where the output has to be the one the Python scripts printed or hashed: repr() of a string,
//! json.dumps() of a string, str.splitlines(), int(s, 0).

/// repr(s) of a Python str
pub fn repr_str(s: &str) -> String {
    let q = if s.contains('\'') && !s.contains('"') { '"' } else { '\'' };
    let mut out = String::new();
    out.push(q);
    for c in s.chars() {
        match c {
            '\\' => out.push_str("\\\\"),
            '\n' => out.push_str("\\n"),
            '\r' => out.push_str("\\r"),
            '\t' => out.push_str("\\t"),
            c if c == q => {
                out.push('\\');
                out.push(c);
            }
            c if (c as u32) < 0x20 || c as u32 == 0x7f => out.push_str(&format!("\\x{:02x}", c as u32)),
            c if (0x80..0xa0).contains(&(c as u32)) || c == '\u{ad}' => out.push_str(&format!("\\x{:02x}", c as u32)),
            c if c.is_control() || matches!(c as u32, 0x2028 | 0x2029 | 0xfeff) => {
                if (c as u32) < 0x10000 {
                    out.push_str(&format!("\\u{:04x}", c as u32))
                } else {
                    out.push_str(&format!("\\U{:08x}", c as u32))
                }
            }
            c => out.push(c),
        }
    }
    out.push(q);
    out
}

/// repr(b) of Python bytes
pub fn repr_bytes(b: &[u8]) -> String {
    let q = if b.contains(&b'\'') && !b.contains(&b'"') { b'"' } else { b'\'' };
    let mut out = String::from("b");
    out.push(q as char);
    for &c in b {
        match c {
            b'\\' => out.push_str("\\\\"),
            b'\n' => out.push_str("\\n"),
            b'\r' => out.push_str("\\r"),
            b'\t' => out.push_str("\\t"),
            c if c == q => {
                out.push('\\');
                out.push(c as char);
            }
            0x20..=0x7e => out.push(c as char),
            c => out.push_str(&format!("\\x{:02x}", c)),
        }
    }
    out.push(q as char);
    out
}

/// json.dumps(s) (ensure_ascii)
pub fn json_str(s: &str) -> String {
    let mut out = String::from("\"");
    for c in s.chars() {
        match c {
            '"' => out.push_str("\\\""),
            '\\' => out.push_str("\\\\"),
            '\n' => out.push_str("\\n"),
            '\r' => out.push_str("\\r"),
            '\t' => out.push_str("\\t"),
            '\u{8}' => out.push_str("\\b"),
            '\u{c}' => out.push_str("\\f"),
            c if (c as u32) < 0x20 || (c as u32) > 0x7e => {
                let mut buf = [0u16; 2];
                for u in c.encode_utf16(&mut buf) {
                    out.push_str(&format!("\\u{:04x}", u));
                }
            }
            c => out.push(c),
        }
    }
    out.push('"');
    out
}

/// str.splitlines()
pub fn splitlines(s: &str) -> Vec<&str> {
    let mut out = Vec::new();
    let mut start = 0;
    let mut it = s.char_indices().peekable();
    while let Some((i, c)) = it.next() {
        let brk = matches!(
            c,
            '\n' | '\r' | '\u{b}' | '\u{c}' | '\u{1c}' | '\u{1d}' | '\u{1e}' | '\u{85}' | '\u{2028}' | '\u{2029}'
        );
        if brk {
            out.push(&s[start..i]);
            let mut end = i + c.len_utf8();
            if c == '\r' {
                if let Some(&(j, '\n')) = it.peek() {
                    it.next();
                    end = j + 1;
                }
            }
            start = end;
        }
    }
    if start < s.len() {
        out.push(&s[start..]);
    }
    out
}

/// int(s, 0): an optional sign, then 0x / 0o / 0b or decimal (no leading zeros), underscores between digits
pub fn int0(s: &str) -> Option<i128> {
    let t = s.trim();
    let (neg, t) = match t.strip_prefix('-') {
        Some(r) => (true, r),
        None => (false, t.strip_prefix('+').unwrap_or(t)),
    };
    let lower = t.to_ascii_lowercase();
    let (radix, digits) = if let Some(r) = lower.strip_prefix("0x") {
        (16, r.strip_prefix('_').unwrap_or(r))
    } else if let Some(r) = lower.strip_prefix("0o") {
        (8, r.strip_prefix('_').unwrap_or(r))
    } else if let Some(r) = lower.strip_prefix("0b") {
        (2, r.strip_prefix('_').unwrap_or(r))
    } else {
        if lower.len() > 1 && lower.starts_with('0') && lower.chars().any(|c| c != '0' && c != '_') {
            return None;
        }
        (10, lower.as_str())
    };
    if digits.is_empty() || digits.starts_with('_') || digits.ends_with('_') || digits.contains("__") {
        return None;
    }
    let v = i128::from_str_radix(&digits.replace('_', ""), radix).ok()?;
    Some(if neg { -v } else { v })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn spellings() {
        assert_eq!(repr_str("ab"), "'ab'");
        assert_eq!(repr_str("it's"), "\"it's\"");
        assert_eq!(repr_str("a\tb\\"), "'a\\tb\\\\'");
        assert_eq!(repr_str("é漢"), "'é漢'");
        assert_eq!(repr_bytes(b"PAT\x00"), "b'PAT\\x00'");
        assert_eq!(json_str("a\"é😀"), "\"a\\\"\\u00e9\\ud83d\\ude00\"");
        assert_eq!(splitlines("a\r\nb\rc\n\nd"), vec!["a", "b", "c", "", "d"]);
        assert_eq!(splitlines("a\n"), vec!["a"]);
        assert_eq!(int0("0x100"), Some(256));
        assert_eq!(int0("-0x1_0"), Some(-16));
        assert_eq!(int0("256"), Some(256));
        assert_eq!(int0("0100"), None);
        assert_eq!(int0("0"), Some(0));
    }
}
