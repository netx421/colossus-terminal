
---

## `install.sh`

```bash
#!/usr/bin/env bash
set -euo pipefail

APP_ID="colossus-terminal"
INSTALL_DIR="${HOME}/.local/share/${APP_ID}"
BIN_DIR="${HOME}/.local/bin"
APP_DIR="${HOME}/.local/share/applications"

DESKTOP_SRC="./colossus-terminal.desktop"
DESKTOP_DST="${APP_DIR}/Terminal.desktop"

echo "== COLOSSUS-TERMINAL :: INSTALLER =="

# 1) sanity check
if [[ ! -f "Makefile" ]] || [[ ! -d "config" ]] || [[ ! -d "src" ]]; then
  echo "[ERROR] Run this from the repo root (where Makefile/config/src live)."
  exit 1
fi

# 2) build
echo "[BUILD] make"
make

# 3) install payload
echo "[DEPLOY] ${INSTALL_DIR}"
mkdir -p "${INSTALL_DIR}"
rsync -a --delete \
  --exclude ".git" \
  --exclude "build" \
  --exclude "*.o" \
  --exclude "${APP_ID}" \
  ./ "${INSTALL_DIR}/"

# Ensure binary is present in install dir (copy the freshly built one)
cp -f "./colossus-terminal" "${INSTALL_DIR}/colossus-terminal"
chmod +x "${INSTALL_DIR}/colossus-terminal"

# 4) wrapper in ~/.local/bin so Exec works anywhere + preserves relative config paths
echo "[WRAPPER] ${BIN_DIR}/colossus-terminal"
mkdir -p "${BIN_DIR}"
cat > "${BIN_DIR}/colossus-terminal" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
APP_DIR="${HOME}/.local/share/colossus-terminal"
cd "${APP_DIR}"
exec "${APP_DIR}/colossus-terminal" "$@"
EOF
chmod +x "${BIN_DIR}/colossus-terminal"

# 5) desktop entry as "Terminal" in the app drawer
echo "[DESKTOP] ${DESKTOP_DST}"
mkdir -p "${APP_DIR}"
if [[ ! -f "${DESKTOP_SRC}" ]]; then
  echo "[ERROR] Missing ${DESKTOP_SRC} in repo root."
  exit 1
fi
cp -f "${DESKTOP_SRC}" "${DESKTOP_DST}"

# 6) refresh desktop database if available (optional)
if command -v update-desktop-database >/dev/null 2>&1; then
  update-desktop-database "${APP_DIR}" >/dev/null 2>&1 || true
fi

echo
echo "== INSTALL COMPLETE =="
echo "Launch from app drawer: Terminal"
echo "Or run: colossus-terminal"
echo
echo "Prompt config:"
echo "  ${INSTALL_DIR}/config/starship-colossus.toml"
