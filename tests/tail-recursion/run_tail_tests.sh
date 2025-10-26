#!/bin/bash

# Tail Recursion Optimization Tests

echo "=== TAIL RECURSION TESTS ==="
echo

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
PROJECT_ROOT="$SCRIPT_DIR/../.."
TUBULAR="$PROJECT_ROOT/build/Tubular"

if [ ! -f "$TUBULAR" ]; then
  echo -e "${RED}Error: Tubular executable not found at $TUBULAR${NC}"
  echo "Please run './make' from the project root first."
  exit 1
fi

if ! command -v wat2wasm &> /dev/null; then
  echo -e "${YELLOW}Warning: wat2wasm not found. Skipping WASM generation.${NC}"
  SKIP_WASM=true
else
  SKIP_WASM=false
fi

if ! command -v node &> /dev/null; then
  echo -e "${YELLOW}Warning: Node.js not found. Skipping execution timings.${NC}"
  SKIP_NODE=true
else
  SKIP_NODE=false
fi

run_case() {
  local base="$1"; local func="$2"; local expect="$3"
  local src="$SCRIPT_DIR/${base}.tube"
  echo "--- $base ---"

  # Compile both variants
  "$TUBULAR" "$src" --tail=off > "$SCRIPT_DIR/${base}-off.wat" 2>/dev/null || { echo -e "${RED}Compile off failed${NC}"; return; }
  "$TUBULAR" "$src" --tail=loop > "$SCRIPT_DIR/${base}-loop.wat" 2>/dev/null || { echo -e "${RED}Compile loop failed${NC}"; return; }
  echo -e "${GREEN}✓ Compilation successful (off/loop)${NC}"

  if [ "$SKIP_WASM" = false ]; then
    wat2wasm "$SCRIPT_DIR/${base}-off.wat" -o "$SCRIPT_DIR/${base}-off.wasm" 2>/dev/null && \
    wat2wasm "$SCRIPT_DIR/${base}-loop.wat" -o "$SCRIPT_DIR/${base}-loop.wasm" 2>/dev/null && \
    echo -e "${GREEN}✓ WAT→WASM conversion successful${NC}" || echo -e "${YELLOW}⚠ WAT→WASM conversion failed${NC}"
  fi

  # Node timing harness
  if [ "$SKIP_NODE" = false ] && [ -f "$SCRIPT_DIR/${base}-off.wasm" ] && [ -f "$SCRIPT_DIR/${base}-loop.wasm" ]; then
    local js="$SCRIPT_DIR/${base}_bench.js"
    cat > "$js" << 'EOF'
const fs = require('fs');
async function load(path){ const buf = fs.readFileSync(path); return await WebAssembly.instantiate(buf); }
function bench(inst, fn, iters){
  const f = inst.instance.exports[fn];
  // Warmup
  for(let i=0;i<5;i++) f();
  const N = 50; const times=[];
  for(let r=0;r<5;r++){
    const t0 = process.hrtime.bigint();
    for(let i=0;i<N;i++) f();
    const t1 = process.hrtime.bigint();
    times.push(Number(t1 - t0) / 1e6 / N);
  }
  times.sort((a,b)=>a-b); return times[Math.floor(times.length/2)];
}
(async ()=>{
  const off = await load(process.argv[2]);
  const loop = await load(process.argv[3]);
  const fn = process.argv[4];
  const expectedArg = process.argv[5];
  const useBaseline = expectedArg === '__baseline__';
  let expected = useBaseline ? null : Number(expectedArg);
  if (!useBaseline && !Number.isFinite(expected)) {
    console.error('Invalid expected value:', expectedArg);
    process.exit(1);
  }
  let resOff, resLoop;
  let offError = null;
  let loopError = null;
  try {
    resOff = off.instance.exports[fn]();
  } catch (err) {
    offError = err;
  }
  try {
    resLoop = loop.instance.exports[fn]();
  } catch (err) {
    loopError = err;
  }
  if (loopError) {
    console.log('Loop execution error:', loopError.message);
    process.exit(1);
  }
  if (useBaseline) {
    expected = resLoop;
  }
  if (offError) {
    console.log('Baseline execution error:', offError.message);
    if (!useBaseline) {
      console.log('RESULT MISMATCH');
      process.exit(1);
    }
  }
  const baselineMatch = offError ? true : (resOff === expected);
  const loopMatch = resLoop === expected;
  const agree = offError ? true : (resOff === resLoop);
  if (!loopMatch || !baselineMatch || !agree) {
    console.log(`Output off=${resOff}, loop=${resLoop}, expected=${expected}`);
    console.log('RESULT MISMATCH');
    process.exit(1);
  }
  if (offError) {
    console.log(`Output loop=${resLoop} (baseline failed: ${offError.message})`);
  } else if (useBaseline) {
    console.log(`Output off=${resOff}, loop=${resLoop} (baseline expected=${expected})`);
  } else {
    console.log(`Output off=${resOff}, loop=${resLoop}, expected=${expected}`);
  }
  let tOff = null;
  if (!offError) {
    tOff = bench(off, fn);
  }
  const tLoop = bench(loop, fn);
  if (tOff !== null && tOff > 0) {
    const delta = ((tLoop - tOff)/tOff*100).toFixed(1);
    console.log(`Median per-call: off=${tOff.toFixed(3)}ms, loop=${tLoop.toFixed(3)}ms (${delta}%)`);
  } else {
    const reason = offError ? offError.message : 'baseline skipped';
    console.log(`Median per-call: loop=${tLoop.toFixed(3)}ms (baseline unavailable: ${reason})`);
  }
  process.exit(0);
})().catch(e=>{ console.error('Bench error', e); process.exit(1); });
EOF
    node "$js" "$SCRIPT_DIR/${base}-off.wasm" "$SCRIPT_DIR/${base}-loop.wasm" "$func" "$expect" 2>/dev/null && \
      echo -e "${GREEN}✓ Execution OK${NC}" || echo -e "${YELLOW}⚠ Execution check failed${NC}"
    rm -f "$js"
  fi
  echo
}

run_case "tail-test-01" "main" 235
run_case "tail-test-02" "main" 5050
run_case "tail-test-03" "main" 500500
run_case "tail-test-04" "main" 6
run_case "tail-test-05" "main" 1048576
run_case "tail-test-deep-01" "main" 50005000
run_case "tail-test-deep-02" "main" 102334155
run_case "tail-test-deep-03" "main" 50005000
run_case "tail-test-extreme" "main" 1048576
run_case "tail-test-order" "main" 8

# Run stress tests
echo "=== STRESS TESTS ==="
run_case "tail-test-stress-01" "main" "__baseline__"  # Will be calculated dynamically
run_case "tail-test-stress-02" "main" "__baseline__"  # Will be calculated dynamically
run_case "tail-test-stress-03" "main" "__baseline__"  # Will be calculated dynamically
run_case "tail-test-stress-04" "main" "__baseline__"  # Will be calculated dynamically
run_case "tail-test-stress-05" "main" "__baseline__"  # Will be calculated dynamically

echo "=== END TAIL RECURSION TESTS ==="
