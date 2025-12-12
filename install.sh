#!/usr/bin/env bash
set -e

# Build artifact name from Makefile
BIN_BUILD="colossus-terminal"
# Name of the installed executable
BIN_INSTALL="colossus-terminal"

echo "[CT-001] INITIALIZING TERMINAL DEPLOYMENT SEQUENCE..."
sleep 1

# --------------------------------------------------------------------
#  PACKAGE ACQUISITION
# --------------------------------------------------------------------
echo "[CT-001] DETECTING HOST DISTRIBUTION..."

if command -v pacman &>/dev/null; then
    echo "[CT-001] ARCH LINUX / MANJARO DETECTED"
    sudo pacman -S --needed --noconfirm \
        base-devel \
        gtk3 \
        vte3 \
        starship \
        pkgconf

elif command -v apt &>/dev/null; then
    echo "[CT-001] DEBIAN / UBUNTU / LINUX MINT DETECTED"
    sudo apt update
    sudo apt install -y \
        build-essential \
        libgtk-3-dev \
        libvte-2.91-dev \
        starship \
        pkg-config

elif command -v dnf &>/dev/null; then
    echo "[CT-001] FEDORA DETECTED"
    sudo dnf install -y \
        gtk3-devel \
        vte291-devel \
        starship \
        pkgconf \
        gcc-c++

elif command -v zypper &>/dev/null; then
    echo "[CT-001] OPENSUSE DETECTED"
    sudo zypper install -y \
        gtk3-devel \
        vte2-devel \
        starship \
        pkgconf \
        gcc-c++

else
    echo "[CT-001] ERROR: Unsupported Linux distribution."
    echo "Install the following dependencies manually:"
    echo "    GTK3, VTE (2.91), starship, pkg-config, C++ build tools"
    exit 1
fi

echo "[CT-001] SYSTEM DEPENDENCIES VERIFIED."

# --------------------------------------------------------------------
#  BUILD PROCESS
# --------------------------------------------------------------------
echo "[CT-002] COMMENCING BUILD OF COLOSSUS TERMINAL NODE..."

make clean || true
make

if [ ! -f "$BIN_BUILD" ]; then
    echo "[CT-002] ERROR: Build failed. Binary '$BIN_BUILD' not found."
    exit 1
fi

echo "[CT-002] BUILD SUCCESSFUL. BINARY READY: $BIN_BUILD"

# --------------------------------------------------------------------
#  INSTALLATION PROCEDURE
# --------------------------------------------------------------------
echo "[CT-003] INSTALLING SYSTEM FILES..."

# Install config directory (bashrc + starship TOML)
if [ -d config ]; then
    sudo mkdir -p /usr/local/share/colossus-terminal/config
    sudo cp -r config/* /usr/local/share/colossus-terminal/config/
else
    echo "[CT-003] WARNING: 'config' directory not found; skipping config install."
fi

# Install executable
sudo cp "$BIN_BUILD" /usr/local/bin/"$BIN_INSTALL"
sudo chmod +x /usr/local/bin/"$BIN_INSTALL"

# Optional: install icon if present
if [ -f colossus-terminal.png ]; then
    echo "[CT-003] INSTALLING APPLICATION ICON..."
    sudo mkdir -p /usr/local/share/icons/hicolor/256x256/apps
    sudo cp colossus-terminal.png \
        /usr/local/share/icons/hicolor/256x256/apps/colossus-terminal.png
else
    echo "[CT-003] NOTICE: colossus-terminal.png not found; skipping icon install."
fi

# Install .desktop entry for drun / app menus
if [ -f colossus-terminal.desktop ]; then
    echo "[CT-003] INSTALLING DESKTOP ENTRY..."
    sudo cp colossus-terminal.desktop /usr/share/applications/colossus-terminal.desktop
else
    echo "[CT-003] WARNING: colossus-terminal.desktop not found; skipping desktop entry install."
fi

echo "[CT-004] INSTALLATION COMPLETE."
echo "[CT-004] EXECUTABLE:     /usr/local/bin/$BIN_INSTALL"
echo "[CT-004] CONFIG:         /usr/local/share/colossus-terminal/config"
echo "[CT-004] DESKTOP ENTRY:  /usr/share/applications/colossus-terminal.desktop"
echo "[CT-004] LAUNCH VIA APP DRAWER: Terminal"
echo "[CT-004] YOU MAY NOW ISSUE: 'colossus-terminal' FROM ANY SHELL."
echo "[CT-004] END OF LINE."
