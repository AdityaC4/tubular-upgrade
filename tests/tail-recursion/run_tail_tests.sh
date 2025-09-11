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
  const expected = Number(process.argv[5]);
  const resOff = off.instance.exports[fn]();
  const resLoop = loop.instance.exports[fn]();
  console.log(`Output off=${resOff}, loop=${resLoop}, expected=${expected}`);
  if (resOff !== expected || resLoop !== expected) {
    console.log('RESULT MISMATCH'); process.exit(1);
  }
  const tOff = bench(off, fn);
  const tLoop = bench(loop, fn);
  const delta = ((tLoop - tOff)/tOff*100).toFixed(1);
  console.log(`Median per-call: off=${tOff.toFixed(3)}ms, loop=${tLoop.toFixed(3)}ms (${delta}%)`);
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

echo "=== END TAIL RECURSION TESTS ==="
