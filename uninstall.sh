#!/usr/bin/env bash
set -euo pipefail

BIN_INSTALL="terminal"
CFG_DIR="/usr/local/share/terminal"
CFG_SUBDIR="/usr/local/share/terminal/config"
DESKTOP="/usr/share/applications/terminal.desktop"
ICON="/usr/local/share/icons/hicolor/256x256/apps/terminal.png"

echo "[CT-RM] INITIALIZING COLOSSUS TERMINAL REMOVAL SEQUENCE..."

need_sudo() {
  if [ "${EUID:-$(id -u)}" -ne 0 ]; then
    sudo "$@"
  else
    "$@"
  fi
}

rm_if_exists() {
  local path="$1"
  if [ -e "$path" ]; then
    echo "[CT-RM] REMOVING: $path"
    need_sudo rm -rf "$path"
  else
    echo "[CT-RM] SKIP (NOT FOUND): $path"
  fi
}

# Safety: show what will be removed
echo "[CT-RM] TARGETS:"
echo "        /usr/local/bin/$BIN_INSTALL"
echo "        $CFG_DIR"
echo "        $DESKTOP"
echo "        $ICON"
echo

# Remove installed binary
rm_if_exists "/usr/local/bin/$BIN_INSTALL"

# Remove installed config payload directory
rm_if_exists "$CFG_DIR"

# Remove desktop entry (only if it looks like ours)
if [ -f "$DESKTOP" ]; then
  if grep -qiE '^Exec=.*\bterminal(\s|$)' "$DESKTOP" || grep -qi 'COLOSSUS' "$DESKTOP"; then
    rm_if_exists "$DESKTOP"
  else
    echo "[CT-RM] NOTICE: $DESKTOP exists but does not look like this project. Leaving it in place."
  fi
else
  echo "[CT-RM] SKIP (NOT FOUND): $DESKTOP"
fi

# Remove icon (optional)
rm_if_exists "$ICON"

# Update caches if tools exist
if command -v update-desktop-database >/dev/null 2>&1; then
  echo "[CT-RM] UPDATING DESKTOP DATABASE..."
  need_sudo update-desktop-database /usr/share/applications >/dev/null 2>&1 || true
fi

if command -v gtk-update-icon-cache >/dev/null 2>&1; then
  echo "[CT-RM] UPDATING ICON CACHE..."
  need_sudo gtk-update-icon-cache -f -t /usr/local/share/icons/hicolor >/dev/null 2>&1 || true
fi

echo "[CT-RM] REMOVAL COMPLETE."
echo "[CT-RM] END OF LINE."
