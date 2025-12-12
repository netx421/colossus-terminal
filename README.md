# COLOSSUS TERMINAL
## FIELD OPERATIONS MANUAL — MONOCHROME EDITION

COLOSSUS-Terminal is a hardened, minimal terminal emulator built for daily duty.

- **Engine:** GTK3 + VTE (stable, mature, fast)
- **Prompt Control:** Starship (project-local TOML)
- **Muscle Memory:** Ctrl+C / Ctrl+V (no Ctrl+Shift gymnastics)
- **Palette:** monochrome (white/grey/black only)
- **Exit Discipline:** `exit` closes the window

---

## OPERATIONAL PROFILE

**Key bindings**
- Ctrl+C → copy (when selection exists) / otherwise SIGINT
- Ctrl+V → paste
- Ctrl+= / Ctrl++ → zoom in *(if enabled in your build)*
- Ctrl+- → zoom out *(if enabled in your build)*
- Ctrl+0 → reset zoom *(if enabled in your build)*

**Config (inside the install)**
- `config/bashrc` loads Starship
- `config/starship-colossus.toml` defines the prompt

---

## DEPENDENCIES

### Arch / Endeavour / Manjaro
```bash
sudo pacman -S --needed gtk3 vte3 starship
