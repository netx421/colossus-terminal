# ~/.config/colossus-terminal/bashrc
# Self-contained COLOSSUS prompt (no starship, no colors)

# Optional: your aliases, etc. (safe, no starship here)
if [ -f "$HOME/.bash_aliases" ]; then
    . "$HOME/.bash_aliases"
fi

# Two-line arrow prompt, monochrome
# \u = user, \h = host, \w = cwd, \A = HH:MM
PS1='\n╭─\u@@\h \w \A\n╰─❯ '
