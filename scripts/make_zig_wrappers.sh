#!/usr/bin/env bash
set -euo pipefail

# Usage: make_zig_wrappers.sh <zig_bin> <zig_target> <multiarch> <out_dir>
ZIG_BIN="$1"
ZIG_TARGET="$2"
MULTIARCH="$3"
OUT_DIR="$4"

mkdir -p "$OUT_DIR"

cat > "${OUT_DIR}/zig-cxx" <<EOF
#!/usr/bin/env bash
args=()
skip_next=0
for a in "\$@"; do
  if [ "\$skip_next" = "1" ]; then
    skip_next=0
    continue
  fi
  if [ "\$a" = "-I/usr/include" ] || [ "\$a" = "-I/usr/include/${MULTIARCH}" ]; then
    continue
  fi
  if [ "\$a" = "-isystem" ]; then
    # Peek via flag: if next arg is /usr/include, handled below; otherwise keep
    args+=("\$a")
    continue
  fi
  args+=("\$a")
done
exec "${ZIG_BIN}" c++ -target "${ZIG_TARGET}" "\${args[@]}" -idirafter /usr/include -idirafter "/usr/include/${MULTIARCH}"
EOF

cat > "${OUT_DIR}/zig-ar" <<EOF
#!/usr/bin/env bash
exec "${ZIG_BIN}" ar "\$@"
EOF

cat > "${OUT_DIR}/zig-ranlib" <<EOF
#!/usr/bin/env bash
exec "${ZIG_BIN}" ranlib "\$@"
EOF

chmod +x "${OUT_DIR}/zig-cxx" "${OUT_DIR}/zig-ar" "${OUT_DIR}/zig-ranlib"
