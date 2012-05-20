# bash completion for npm

_npm_completion () {
  local si="$IFS"
  IFS=$'\n' COMPREPLY=($(COMP_CWORD="$COMP_CWORD" \
                         COMP_LINE="$COMP_LINE" \
                         COMP_POINT="$COMP_POINT" \
                         npm completion -- "${COMP_WORDS[@]}" \
                         2>/dev/null)) || return $?
  IFS="$si"
}
complete -F _npm_completion npm
