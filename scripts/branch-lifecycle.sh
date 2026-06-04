#!/usr/bin/env bash
# Branch Lifecycle Automation — Metro Design
set -euo pipefail

RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; CYAN='\033[0;36m'; NC='\033[0m'
info() { echo -e "  ${CYAN}INFO${NC} $1"; }
pass() { echo -e "  ${GREEN}PASS${NC} $1"; }
warn() { echo -e "  ${YELLOW}WARN${NC} $1"; }
fail() { echo -e "  ${RED}FAIL${NC} $1"; }

DRY_RUN=false
STALE_DAYS=90
PRUNE_REMOTE=false
ACTION="report"

usage() {
  cat <<EOF
Usage: $0 [options]

Branch lifecycle management for Metro Design repositories.

Actions:
  report              Show branch health report (default)
  check-naming       Validate branch naming conventions

Options:
  --dry-run           Show what would be done without making changes
  --stale-days DAYS   Days of inactivity to consider branch stale (default: 90)
  -h, --help          Show this help

Branch naming conventions:
  main               Default branch (protected)
  feature/<name>     New features
  fix/<name>         Bug fixes
  refactor/<name>    Code refactoring
  docs/<name>        Documentation
  chore/<name>       Maintenance

Examples:
  $0 report
  $0 check-naming
  $0 report --stale-days 60
EOF
  exit 0
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    report) ACTION="report"; shift ;;
    check-naming) ACTION="check-naming"; shift ;;
    --dry-run) DRY_RUN=true; shift ;;
    --stale-days) STALE_DAYS="$2"; shift 2 ;;
    -h|--help) usage ;;
    *) echo "Unknown option: $1"; usage ;;
  esac
done

REPO_ROOT=$(git rev-parse --show-toplevel 2>/dev/null || { echo "Not a git repository."; exit 1; })
cd "$REPO_ROOT"

ensure_fetch() {
  if ! git remote get-url origin &>/dev/null; then
    warn "No remote 'origin' configured."
    return 1
  fi
  return 0
}

report_branches() {
  echo -e "${CYAN}── Branch Report ──────────────────────────────────────────${NC}"
  echo ""

  current_branch=$(git branch --show-current)
  echo "  Current branch : $current_branch"
  echo ""

  local_branches=$(git branch --list --format='%(refname:short)')
  remote_refs=$(git branch -r 2>/dev/null | grep -v '\->' | sed 's/^.\{2\}//' || true)

  echo "  Local branches:"
  local_count=0
  while IFS= read -r branch; do
    [ -z "$branch" ] && continue
    local_count=$((local_count+1))
    marker=" "
    [ "$branch" = "$current_branch" ] && marker="*"
    last_commit=$(git log -1 --format='%ci' "$branch" 2>/dev/null || echo "unknown")
    echo "    $marker $branch (last: $last_commit)"
  done <<< "$local_branches"
  echo "    Total: $local_count local branch(es)"

  echo ""
  echo "  Stale local branches (>${STALE_DAYS}d):"
  stale_count=0
  while IFS= read -r branch; do
    [ -z "$branch" ] && continue
    [ "$branch" = "main" ] || [ "$branch" = "master" ] && continue
    last_ts=$(git log -1 --format='%ct' "$branch" 2>/dev/null || echo "0")
    now=$(date +%s)
    age=$(( (now - last_ts) / 86400 ))
    if [ "$age" -gt "$STALE_DAYS" ]; then
      stale_count=$((stale_count+1))
      warn "'$branch' — ${age}d since last commit"
    fi
  done <<< "$local_branches"
  [ "$stale_count" -eq 0 ] && pass "No stale branches found."
  echo ""
}

check_branch_naming() {
  echo -e "${CYAN}── Branch Naming Convention Check ─────────────────────────${NC}"
  echo ""

  protected_branches="^(main|master|develop)$"
  valid_prefixes="^(feature|fix|refactor|docs|chore|ci|test|release)/.+"

  local_branches=$(git branch --list --format='%(refname:short)')
  violations=0

  while IFS= read -r branch; do
    [ -z "$branch" ] && continue
    if echo "$branch" | grep -qE "$protected_branches"; then
      pass "'$branch' — protected branch (OK)."
    elif echo "$branch" | grep -qE "$valid_prefixes"; then
      pass "'$branch' — naming convention OK."
    else
      fail "'$branch' — violates naming convention!"
      violations=$((violations+1))
    fi
  done <<< "$local_branches"

  echo ""
  if [ "$violations" -eq 0 ]; then
    pass "All branches follow naming conventions."
  else
    fail "$violations branch(es) violate naming conventions."
  fi
  echo ""

  echo "  Expected patterns:"
  echo "    feature/<name>    fix/<name>       refactor/<name>"
  echo "    docs/<name>       chore/<name>     ci/<name>"
  echo "    test/<name>       release/<name>"
}

check_merged_branches() {
  echo -e "${CYAN}── Merged Branch Audit ────────────────────────────────────${NC}"
  echo ""

  merged=$(git branch --merged main 2>/dev/null | grep -v '^\*' | grep -vE '^(main|master)$' || true)
  if [ -z "$merged" ] || [ "$(echo "$merged" | grep -c .)" -eq 0 ]; then
    pass "No merged branches to clean up."
  else
    echo "$merged" | while IFS= read -r branch; do
      [ -z "$branch" ] && continue
      if [ "$DRY_RUN" = true ]; then
        warn "[DRY RUN] Would delete merged branch: $branch"
      else
        warn "Merged branch (needs cleanup): $branch"
      fi
    done
  fi
  echo ""
}

case "$ACTION" in
  report)
    ensure_fetch || true
    report_branches
    check_merged_branches
    echo -e "${CYAN}── Next steps ───────────────────────────────────────────${NC}"
    echo "  - Run '$0 check-naming' to validate conventions"
    echo "  - To delete stale branches: git branch -d <branch>"
    echo "  - Update STALE_DAYS with --stale-days <N>"
    echo ""
    ;;
  check-naming)
    check_branch_naming
    ;;
esac
