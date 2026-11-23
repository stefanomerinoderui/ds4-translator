# Generic wrapper: asks the ds4-ctl binary itself for its command list and
# per-command argument completions (via hidden --commands/--complete-args
# queries) instead of hardcoding them here. Mirrors ds4-ctl-completion.bash;
# nothing in this file ever needs editing when commands are added.
complete -c ds4-ctl -f

# First argument: the command name.
complete -c ds4-ctl \
    -n 'test (count (commandline -opc)) -eq 1' \
    -a '(ds4-ctl --commands 2>/dev/null)'

# Later arguments: pass the command plus every word already typed after it,
# so completion can depend on position (e.g. `set-backend` offers the
# controller type first, then the backend).
complete -c ds4-ctl \
    -n 'test (count (commandline -opc)) -ge 2' \
    -a '(ds4-ctl --complete-args (commandline -opc)[2..-1] 2>/dev/null)'
