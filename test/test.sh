#!/usr/bin/env bash
set -uo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

print_kv() {
  printf "  %-16s %s\n" "$1" "$2"
}

print_section() {
  printf "\n%-64s\n" "  $1"
  printf "  %s\n" "------------------------------------------------------------"
}

print_system_info() {
  local cpu_model="unknown"
  local cpu_cores="unknown"
  local cpu_freq="unknown"
  local os_name="unknown"
  local total_mem="unknown"
  local free_mem="unknown"
  local loadavg="unknown"

  if [[ -f "/proc/cpuinfo" ]]; then
    cpu_model="$(awk -F: '/model name/ {gsub(/^[ \t]+/, "", $2); print $2; exit}' /proc/cpuinfo)"
    cpu_cores="$(grep -c '^processor' /proc/cpuinfo)"
    cpu_freq="$(awk -F: '/cpu MHz/ {gsub(/^[ \t]+/, "", $2); printf "%.2f MHz", $2; exit}' /proc/cpuinfo)"
  fi
  if [[ -f "/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq" ]]; then
    cpu_freq="$(awk '{printf "%.2f GHz", $1/1000000}' /sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq)"
  fi

  if [[ -f "/proc/meminfo" ]]; then
    total_mem="$(awk '/MemTotal/ {printf "%.2f GB", $2/1024/1024}' /proc/meminfo)"
    free_mem="$(awk '/MemAvailable/ {printf "%.2f GB", $2/1024/1024}' /proc/meminfo)"
  fi

  if [[ -f "/etc/os-release" ]]; then
    os_name="$(awk -F= '/PRETTY_NAME/ {gsub(/"/, "", $2); print $2; exit}' /etc/os-release)"
  elif [[ -f "/etc/lsb-release" ]]; then
    os_name="$(awk -F= '/DISTRIB_DESCRIPTION/ {gsub(/"/, "", $2); print $2; exit}' /etc/lsb-release)"
  fi

  if [[ -f "/proc/loadavg" ]]; then
    loadavg="$(awk '{print $1 " / " $2 " / " $3}' /proc/loadavg)"
  fi

  echo
  echo "================================================================"
  echo "  Server Environment"
  echo "================================================================"

  print_section "CPU"
  print_kv "Model" "$cpu_model"
  print_kv "Cores" "$cpu_cores"
  print_kv "Frequency" "$cpu_freq"

  print_section "Memory"
  print_kv "Total" "$total_mem"
  print_kv "Available" "$free_mem"

  print_section "System"
  print_kv "OS" "$os_name"
  print_kv "Kernel" "$(uname -r)"
  print_kv "Arch" "$(uname -m)"
  print_kv "Load avg" "$loadavg"

  if command -v df >/dev/null; then
    print_section "Disk"
    df -h --output=source,size,used,avail,pcent,target / "$ROOT_DIR" 2>/dev/null \
      | awk 'NR==1 {printf "  %-24s %8s %8s %8s %6s  %s\n", $1, $2, $3, $4, $5, $6; next}
             !seen[$6]++ {printf "  %-24s %8s %8s %8s %6s  %s\n", $1, $2, $3, $4, $5, $6}'
  fi

  echo "================================================================"
  echo
}

REPEATS="${REPEATS:-3}"
THREADS="${THREADS:-1}"
BUILD_PRESET="${BUILD_PRESET:-}"
SKIP_BUILD="${SKIP_BUILD:-1}"
SKIP_CHECK="${SKIP_CHECK:-0}"

while [[ $# -gt 0 ]]; do
  case "$1" in
    -n|--repeat|--repeats)
      REPEATS="$2"
      shift 2
      ;;
    -t|--thread|--threads)
      THREADS="$2"
      shift 2
      ;;
    --build-preset)
      BUILD_PRESET="$2"
      shift 2
      ;;
    --skip-build)
      SKIP_BUILD=1
      shift
      ;;
    --skip-check)
      SKIP_CHECK=1
      shift
      ;;
    -h|--help)
      cat <<'EOF'
Usage: bash test/test.sh [options]

Options:
  -n, --repeats N       Run each rule N times (default: 3)
  -t, --threads N       Pass -thread N to trace (default: 1)
  --build-preset NAME   Build once using the given CMake preset
  --skip-build          Do not build; use existing bin/trace (default)
  --skip-check          Skip checker validation

By default this script runs with --skip-build. Pass --build-preset NAME to build once.

Environment variables with the same names are also supported:
  REPEATS, THREADS, BUILD_PRESET, SKIP_BUILD, SKIP_CHECK
EOF
      exit 0
      ;;
    *)
      echo "Unknown argument: $1" >&2
      exit 2
      ;;
  esac
done

print_system_info

if ! [[ "$REPEATS" =~ ^[1-9][0-9]*$ ]]; then
  echo "REPEATS must be a positive integer, got: $REPEATS" >&2
  exit 2
fi

if ! [[ "$THREADS" =~ ^[1-9][0-9]*$ ]]; then
  echo "THREADS must be a positive integer, got: $THREADS" >&2
  exit 2
fi

mkdir -p logs results solution/Public solution/Hidden

declare -a CASES=(
  "Public|case1_small_q1|instance/case/Public/case1_small_layout.txt|instance/Rule/Public/public_small_rule1.txt|answer/Public/small/case1_small_q1.txt|solution/Public/case1_small_layout_q1.txt"
  "Public|case1_small_q2|instance/case/Public/case1_small_layout.txt|instance/Rule/Public/public_small_rule2.txt|answer/Public/small/case1_small_q2.txt|solution/Public/case1_small_layout_q2.txt"
  "Public|case1_small_q3|instance/case/Public/case1_small_layout.txt|instance/Rule/Public/public_small_rule3.txt|answer/Public/small/case1_small_q3_0912.txt|solution/Public/case1_small_layout_q3.txt"
  "Public|case1_large_q1|instance/case/Public/case1_large_0909b_layout.txt|instance/Rule/Public/public_large_rule1.txt|answer/Public/large/case1_large_0909b_q1.txt|solution/Public/case1_large_layout_q1.txt"
  "Public|case1_large_q2|instance/case/Public/case1_large_0909b_layout.txt|instance/Rule/Public/public_large_rule2.txt|answer/Public/large/case1_large_0909b_q2.txt|solution/Public/case1_large_layout_q2.txt"
  "Public|case1_large_q3|instance/case/Public/case1_large_0909b_layout.txt|instance/Rule/Public/public_large_rule3.txt|answer/Public/large/case1_large_gate_q3_0912.txt|solution/Public/case1_large_layout_q3.txt"
  "Hidden|hidden_case_q1|instance/case/Hidden/hidden_case_layout.txt|instance/Rule/Hidden/hidden_case_rule_q1.txt|answer/Hidden/hidden_case_q1.txt|solution/Hidden/hidden_case_layout_q1.txt"
  "Hidden|hidden_case_q2|instance/case/Hidden/hidden_case_layout.txt|instance/Rule/Hidden/hidden_case_rule_q2.txt|answer/Hidden/hidden_case_q2.txt|solution/Hidden/hidden_case_layout_q2.txt"
  "Hidden|hidden_case_q3|instance/case/Hidden/hidden_case_layout.txt|instance/Rule/Hidden/hidden_case_rule_q3.txt|answer/Hidden/hidden_case_q3.txt|solution/Hidden/hidden_case_layout_q3.txt"
)

build_args=()
if [[ -n "$BUILD_PRESET" ]]; then
  build_args+=(--build-preset "$BUILD_PRESET")
elif [[ "$SKIP_BUILD" == "1" ]]; then
  build_args+=(--skip-build)
fi

first_run=1
total_runs=0
failed_runs=0
skipped_cases=0

analyze_case() {
  local case_name="$1"
  echo "------------------------------------------------------------"
  echo "Analyzing logs for $case_name"
  echo "------------------------------------------------------------"
  python3 test/analysis.py "logs/${case_name}_t${THREADS}"*.txt \
    -o "results/${case_name}_t${THREADS}.csv"
}

for case_spec in "${CASES[@]}"; do
  IFS='|' read -r group case_name layout rule ans output <<< "$case_spec"

  missing=()
  [[ -f "$layout" ]] || missing+=("$layout")
  [[ -f "$rule" ]] || missing+=("$rule")
  if [[ "$SKIP_CHECK" != "1" && ! -f "$ans" ]]; then
    missing+=("$ans")
  fi

  if (( ${#missing[@]} > 0 )); then
    echo "⚠️  Skip $case_name, missing files:"
    printf '   - %s\n' "${missing[@]}"
    skipped_cases=$((skipped_cases + 1))
    continue
  fi

  for repeat in $(seq 1 "$REPEATS"); do
    repeat_label="$(printf "%02d" "$repeat")"
    log_file="logs/${case_name}_t${THREADS}_r${repeat_label}.txt"
    echo "============================================================"
    echo "Running $case_name repeat $repeat/$REPEATS thread=$THREADS"
    echo "Log: $log_file"
    echo "============================================================"

    current_build_args=("${build_args[@]}")
    if [[ "$first_run" == "0" ]]; then
      current_build_args=(--skip-build)
    fi

    ans_args=()
    current_check_args=()
    if [[ "$SKIP_CHECK" != "1" && "$repeat" == "1" ]]; then
      ans_args=(-ans "$ans")
    else
      current_check_args=(--skip-check)
    fi

    set +e
    python3 test/run_trace.py \
      -layout "$layout" \
      -rule "$rule" \
      -output "$output" \
      "${ans_args[@]}" \
      -thread "$THREADS" \
      "${current_build_args[@]}" \
      "${current_check_args[@]}" 2>&1 | tee "$log_file"
    status=${PIPESTATUS[0]}
    set -e

    total_runs=$((total_runs + 1))
    first_run=0
    if [[ "$status" -ne 0 ]]; then
      echo "❌ Run failed: $case_name repeat $repeat (exit=$status)"
      failed_runs=$((failed_runs + 1))
    fi
  done

  analyze_case "$case_name"
done

echo "============================================================"
echo "Final CSV files"
echo "============================================================"
echo "Batch finished: total_runs=$total_runs failed_runs=$failed_runs skipped_cases=$skipped_cases"
echo "Per-case CSV files: results/<case>_t${THREADS}.csv"
echo "============================================================"

if [[ "$failed_runs" -ne 0 ]]; then
  exit 1
fi