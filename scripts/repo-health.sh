#!/usr/bin/env bash
# Repo Health Dashboard — Metro Design
set -euo pipefail

RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; CYAN='\033[0;36m'; NC='\033[0m'
pass() { echo -e "  ${GREEN}PASS${NC} $1"; }
warn() { echo -e "  ${YELLOW}WARN${NC} $1"; }
fail() { echo -e "  ${RED}FAIL${NC} $1"; }
info() { echo -e "  ${CYAN}INFO${NC} $1"; }

REPO_ROOT=$(git rev-parse --show-toplevel 2>/dev/null || echo ".")
cd "$REPO_ROOT"

total_checks=0; passed=0; warnings=0; failures=0
tally_pass() { total_checks=$((total_checks+1)); passed=$((passed+1)); }
tally_warn() { total_checks=$((total_checks+1)); warnings=$((warnings+1)); }
tally_fail() { total_checks=$((total_checks+1)); failures=$((failures+1)); }

echo -e "${CYAN}══════════════════════════════════════════════════════════════${NC}"
echo -e "${CYAN}   Metro Design — Repo Health Dashboard${NC}"
echo -e "${CYAN}   $(date -u '+%Y-%m-%d %H:%M UTC')${NC}"
echo -e "${CYAN}══════════════════════════════════════════════════════════════${NC}"
echo ""

echo -e "${CYAN}── Repository Info ───────────────────────────────────────────${NC}"
echo "  Repository : $(basename $(git remote get-url origin 2>/dev/null || echo 'unknown'))"
echo "  Remote     : $(git remote get-url origin 2>/dev/null || echo 'none')"
echo "  Branch     : $(git branch --show-current)"
echo "  HEAD       : $(git rev-parse --short HEAD 2>/dev/null || echo 'unknown')"
echo "  Tag        : $(git describe --tags --always 2>/dev/null || echo 'none')"
echo ""

echo -e "${CYAN}── 1. Branch Health ──────────────────────────────────────────${NC}"

stale_days=90
branches=$(git branch -r 2>/dev/null | grep -v '\->' || true)
branch_count=$(echo "$branches" | grep -c . || echo 0)
echo "  Total remote branches: $branch_count"

if [ "$branch_count" -le 1 ]; then
  info "Only one branch exists — no stale branch analysis needed."
  tally_pass
else
  stale_count=0
  while IFS= read -r branch; do
    branch_name=$(echo "$branch" | sed 's/^.\{2\}//')
    last_commit=$(git log -1 --format='%ci' "$branch_name" 2>/dev/null || echo "")
    if [ -n "$last_commit" ]; then
      last_ts=$(date -d "$last_commit" +%s 2>/dev/null || date -j -f "%Y-%m-%d %H:%M:%S %z" "$last_commit" +%s 2>/dev/null)
      now=$(date +%s)
      age=$(( (now - last_ts) / 86400 ))
      if [ "$age" -gt "$stale_days" ]; then
        warn "Stale branch: $branch_name (${age}d since last commit)"
        stale_count=$((stale_count+1))
      fi
    fi
  done <<< "$branches"
  if [ "$stale_count" -eq 0 ]; then
    pass "No stale branches (>${stale_days}d inactive)."
    tally_pass
  else
    fail "Found $stale_count stale branch(es)."
    tally_fail
  fi
fi

echo ""

echo -e "${CYAN}── 2. Large Files ────────────────────────────────────────────${NC}"

large_threshold=1024
large_files=$(git rev-list --objects --all 2>/dev/null | git cat-file --batch-check='%(objecttype) %(objectname) %(objectsize) %(rest)' | awk -v limit="$((large_threshold*1024))" '/^blob/ && $3 > limit {print int($3/1024) " KB  " $4}' | sort -rn | head -10)

if [ -z "$large_files" ]; then
  pass "No files larger than ${large_threshold}KB in git history."
  tally_pass
else
  warn "Large files in git history (>${large_threshold}KB):"
  echo "$large_files" | while IFS= read -r line; do echo "    $line"; done
  tally_warn
fi

echo ""

echo -e "${CYAN}── 3. .gitignore Health ──────────────────────────────────────${NC}"

ignore_checks=0
if [ -f ".gitignore" ]; then
  pass ".gitignore exists."
  ignore_checks=$((ignore_checks+1))

  tracked_ignored=$(git ls-files --cached --ignored --exclude-from=.gitignore 2>/dev/null | head -5)
  if [ -n "$tracked_ignored" ]; then
    warn "Tracked files matching .gitignore patterns (should be untracked):"
    echo "$tracked_ignored" | while IFS= read -r f; do echo "    $f"; done
    tally_warn
  else
    pass "No files tracked that match .gitignore patterns."
    ignore_checks=$((ignore_checks+1))
  fi

  unignored_dirs=("node_modules" ".next" "build" "__pycache__")
  for d in "${unignored_dirs[@]}"; do
    if git check-ignore "$d" &>/dev/null; then
      :
    else
      if [ -d "$d" ]; then
        warn "Directory '$d' exists but is NOT in .gitignore."
        tally_warn
      fi
    fi
  done
else
  fail ".gitignore is missing!"
  tally_fail
fi

echo ""

echo -e "${CYAN}── 4. Repository Integrity ───────────────────────────────────${NC}"

merge_conflicts=$(git ls-files -u 2>/dev/null | head -5)
if [ -n "$merge_conflicts" ]; then
  fail "Unresolved merge conflicts detected!"
  tally_fail
else
  pass "No merge conflicts."
  tally_pass
fi

uncommitted=$(git status --porcelain 2>/dev/null | wc -l)
if [ "$uncommitted" -gt 0 ]; then
  warn "$uncommitted uncommitted file(s)."
  tally_warn
else
  pass "Working tree is clean."
  tally_pass
fi

echo ""

echo -e "${CYAN}── 5. Binary & Secret Detection ─────────────────────────────${NC}"

binary_extensions="\.(exe|dll|so|dylib|jar|war|class|pyc|o|obj|lib|a|out)$"
tracked_binaries=$(git ls-files 2>/dev/null | grep -iE "$binary_extensions" | head -5)
if [ -n "$tracked_binaries" ]; then
  warn "Tracked files with binary extensions:"
  echo "$tracked_binaries" | while IFS= read -r f; do echo "    $f"; done
  tally_warn
else
  pass "No tracked binary files."
  tally_pass
fi

if git ls-files 2>/dev/null | grep -qiE '\.(env|cred|key|pem|p12|pfx|secret)$' | head -5; then
  fail "Potential secret files tracked in git!"
  tally_fail
else
  pass "No secret files tracked."
  tally_pass
fi

echo ""

echo -e "${CYAN}── 6. CI/CD Configuration ────────────────────────────────────${NC}"

if [ -f ".github/workflows/ci.yml" ]; then
  pass "CI workflow configured."
  tally_pass
else
  fail "No CI workflow found!"
  tally_fail
fi

if [ -f ".pre-commit-config.yaml" ]; then
  pass "Pre-commit hooks configured."
  tally_pass
else
  warn "No pre-commit configuration."
  tally_warn
fi

if [ -f ".github/pull_request_template.md" ]; then
  pass "PR template exists."
  tally_pass
else
  warn "No PR template."
  tally_warn
fi

echo ""

echo -e "${CYAN}── 7. Commit Conventions ─────────────────────────────────────${NC}"

conventional_types="feat|fix|refactor|test|docs|ci|chore|perf|style|build|revert"
recent_messages=$(git log --oneline -20 --format='%s' 2>/dev/null)
non_conforming=0
while IFS= read -r msg; do
  [ -z "$msg" ] && continue
  if ! echo "$msg" | grep -qiE "^(${conventional_types})(\(.+\))?:"; then
    non_conforming=$((non_conforming+1))
  fi
done <<< "$recent_messages"

if [ "$non_conforming" -gt 0 ]; then
  warn "$non_conforming of last 20 commits do not follow Conventional Commits."
  tally_warn
else
  pass "Recent commits follow Conventional Commits."
  tally_pass
fi

echo ""

echo -e "${CYAN}── 8. Repository Size ────────────────────────────────────────${NC}"

if command -v git &>/dev/null; then
  repo_size=$(git count-objects -vH 2>/dev/null | grep "size-pack" | awk '{print $2}')
  echo "  Packed size: $repo_size"
fi
file_count=$(git ls-files 2>/dev/null | wc -l)
echo "  Tracked files: $file_count"

echo ""

echo -e "${CYAN}══════════════════════════════════════════════════════════════${NC}"
echo -e "  ${CYAN}Summary:${NC} $total_checks checks"
echo -e "  ${GREEN}$passed passed${NC}"
echo -e "  ${YELLOW}$warnings warnings${NC}"
echo -e "  ${RED}$failures failures${NC}"
echo -e "${CYAN}══════════════════════════════════════════════════════════════${NC}"

exit $(( failures > 0 ? 1 : 0 ))
