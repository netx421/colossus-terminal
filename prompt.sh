# ~/.config/colossus-terminal/prompt.sh
# Monochrome two-line prompt with arrow bar and time

# Use bash's PROMPT_COMMAND to update things each time
__colossus_prompt() {
    local exit_code=$?

    # Short forms
    local user="\u"
    local host="\h"
    local cwd="\w"
    local time24="\A"   # HH:MM
    # Or use \t for HH:MM:SS if you prefer

    # Optional: show git branch in parentheses if in repo
    local git_branch=""
    if command -v git >/dev/null 2>&1; then
        local b
        b=$(git rev-parse --abbrev-ref HEAD 2>/dev/null)
        if [ -n "$b" ] && [ "$b" != "HEAD" ]; then
            git_branch=" ($b)"
        fi
    fi

    # You can add versions here later if you want:
    #   python version, node version, etc., and keep it monochrome.

    # First line (status + user@host + cwd + git + time)
    # Using only grayscale-capable attributes: bold/underline
    PS1="\n"
    PS1+="\[\e[1m\]╭─${user}@${host}\[\e[0m\] "    # bold user@host
    PS1+="\[\e[0m\]${cwd}${git_branch} "          # cwd + (branch)
    PS1+="\[\e[1m\]${time24}\[\e[0m\]\n"          # bold time

    # Second line (arrow prompt)
    PS1+="\[\e[1m\]╰─❯ \[\e[0m\]"

    # Save exit code in case you want to color/change based on it later
    return $exit_code
}

PROMPT_COMMAND=__colossus_prompt
