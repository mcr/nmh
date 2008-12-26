# Common helper routines for test shell scripts -- intended to be sourced by them
test_skip ()
{
  WHY="$1"
  echo "$Test $0 SKIP ($WHY)"
  exit 120
}

# portable implementation of 'which' utility
findprog()
{
  FOUND=
  PROG="$1"
  IFS_SAVE="$IFS"
  IFS=:
  for D in $PATH; do
    if [ -z "$D" ]; then
      D=.
    fi
    if [ -f "$D/$PROG" ] && [ -x "$D/$PROG" ]; then
      printf '%s\n' "$D/$PROG"
      break
    fi
  done
  IFS="$IFS_SAVE"
}

require_prog ()
{
  if [ -z "$(findprog $1)" ]; then
    test_skip "missing $1"
  fi
}
