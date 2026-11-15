#!/bin/bash
# ======================================================================
# Okin test suite
#
# Usage:
#   bash core/test_cases.sh                 # interpreter (default)
#   bash core/test_cases.sh vm              # bytecode VM
#   bash core/test_cases.sh both            # run each test on both backends
#   ONLY=loops bash core/test_cases.sh vm   # run only sections whose name contains "loops"
#
# Adding a test:
#   t      'program' 'expected stdout' ['label']     stdout must match exactly
#   t_in   'program' 'stdin text' 'expected' ['label']   same, with stdin supplied
#   t_err  'program' 'label'                         must exit 1..123 with EMPTY stdout
#                                                    (fails on exit 0, hang, or abort/segfault)
# Programs are single-quoted, so $0 and " need no escaping.
# ======================================================================

exe_path="./build/okin"
mode="${1:-interp}"          # interp | vm | both
ONLY="${ONLY:-}"

passed=0
total=0
skipped_section=0
current_section=""
failures=()

GREEN='\033[0;32m'; RED='\033[0;31m'; YELLOW='\033[0;33m'; NC='\033[0m'

# ----------------------------------------------------------------------
# helpers
# ----------------------------------------------------------------------

# run_prog <program> <backend: vm|interp> <stdin>
run_prog() {
	local flag=""; [[ "$2" == vm ]] && flag="-vm"
	timeout 5 "$exe_path" "$1" $flag 2>/dev/null <<<"$3"
}

primary_backend() { [[ "$mode" == vm ]] && echo vm || echo interp; }

record_pass() { echo -e "${GREEN}[PASS]${NC} $1"; ((passed++)); ((total++)); }
record_fail() { echo -e "${RED}[FAIL]${NC} $1"; failures+=("[$current_section] $1"); ((total++)); }

section() {
	current_section="$1"
	if [[ -n "$ONLY" && "$1" != *"$ONLY"* ]]; then skipped_section=1; return; fi
	skipped_section=0
	echo; echo -e "${YELLOW}== $1 ==${NC}"
}

# t <program> <expected> [label]
t() { t_in "$1" "" "$2" "${3:-$1}"; }

# t_in <program> <stdin> <expected> [label]
t_in() {
	[[ $skipped_section -eq 1 ]] && return
	local prog="$1" input="$2" exp="$3" label="${4:-$1}"

	if [[ "$mode" == both ]]; then
		local a b
		a=$(run_prog "$prog" vm "$input"); b=$(run_prog "$prog" interp "$input")
		a=${a//$'\r\n'/$'\n'}; b=${b//$'\r\n'/$'\n'}
		if   [[ "$a" == "$exp" && "$b" == "$exp" ]]; then record_pass "$label"
		else
			record_fail "$label"
			echo "  expected: $exp"
			echo "  vm:       $a"
			echo "  interp:   $b"
		fi
		return
	fi

	local out; out=$(run_prog "$prog" "$(primary_backend)" "$input")
	out=${out//$'\r\n'/$'\n'}
	if [[ "$out" == "$exp" ]]; then record_pass "$label"
	else
		record_fail "$label"
		echo "  expected: $exp"
		echo "  got:      $out"
	fi
}

# t_err <program> <label>
t_err() {
	[[ $skipped_section -eq 1 ]] && return
	local backends=("$(primary_backend)"); [[ "$mode" == both ]] && backends=(vm interp)
	local b out code ok=1 detail=""
	for b in "${backends[@]}"; do
		out=$(run_prog "$1" "$b" ""); code=$?
		out=${out//$'\r\n'/$'\n'}
		if ! [[ $code -ge 1 && $code -lt 124 && -z "$out" ]]; then
			ok=0; detail+=" [$b: exit $code, stdout '$out']"
		fi
	done
	if [[ $ok -eq 1 ]]; then record_pass "$2"; else record_fail "$2$detail"; fi
}

# ======================================================================
# TESTS
# ======================================================================

# ----------------------------------------------------------------------
section "basics: output, variables"
# ----------------------------------------------------------------------
t '192~WRITELN<"Hello, World!">'                       'Hello, World!'
t '2<X, 10>;192~WRITELN<X>'                            '10'
t '2<X, 10>;3<X, 20>;192~WRITELN<X>'                   '20'
t '# COMMENT'                                          ''
t '192~WRITELN<@2.5>'                                  '2.5'
t '192~WRITELN<@-3.5>'                                 '-3.5'
t '192~WRITE<"a">;192~WRITE<"b">;192~WRITELN<"c">'     'abc'

# ----------------------------------------------------------------------
section "lexer: specials inside strings"
# ----------------------------------------------------------------------
t '192~WRITELN<"a;b,c|d<e>">'                          'a;b,c|d<e>'
t '192~WRITELN<"a # b">'                               'a # b'

# ----------------------------------------------------------------------
section "arithmetic"
# ----------------------------------------------------------------------
t '2<A, 10>;2<B, 3>;2<R, 0>;68<A, B, R>;192~WRITELN<R>'   '1'      # MOD
t '2<A, 7>;2<B, 6>;2<R, 0>;66<A, B, R>;192~WRITELN<R>'    '42'     # MUL
t '2<A, 0>;2<B, 5>;2<R, 0>;65<A, B, R>;192~WRITELN<R>'    '-5'     # SUB
t '2<A, 10>;2<B, 3>;2<R, 0>;67<A, B, R>;192~WRITELN<R>'   '3'      # DIV (int)
t '2<R,0>;67<7,2,R>;192~WRITELN<R>'                        '3'      # int / int
t '2<R,0>;67<@7.0,2,R>;192~WRITELN<R>'                     '3.5'    # float / int
t '2<R,0>;64<1,@2.5,R>;192~WRITELN<R>'                     '3.5'    # int + float
t '2<R,0>;68<-7,3,R>;192~WRITELN<R>'                       '-1'     # C-style modulo
t '2<N,0>;65<@0.0,@3.5,N>;192~WRITELN<N>'                  '-3.5'
t '2<R,0>;64<9007199254740992,1,R>;192~WRITELN<R>'         '9007199254740993'   # int stays exact

# ----------------------------------------------------------------------
section "math library"
# ----------------------------------------------------------------------
t '2<R, 0>;224~POW<2, 10, R>;192~WRITELN<R>'           '1024'
t '2<R, 0>;224~SQRT<144, R>;192~WRITELN<R>'            '12'
t '2<R, 0>;224~ABS<-42, R>;192~WRITELN<R>'             '42'
t '2<R, 0>;224~MIN<10, 3, R>;192~WRITELN<R>'           '3'
t '2<R, 0>;224~MAX<10, 3, R>;192~WRITELN<R>'           '10'
t '2<R, 0>;224~FLOOR<@3.9, R>;192~WRITELN<R>'          '3'
t '2<R, 0>;224~CEIL<@3.1, R>;192~WRITELN<R>'           '4'
t '2<R,0>;224~FLOOR<@-3.5,R>;192~WRITELN<R>'           '-4'     # literal
t '2<R,0>;224~CEIL<@-3.5,R>;192~WRITELN<R>'            '-3'
t '2<N,0>;65<@0.0,@3.5,N>;2<R,0>;224~FLOOR<N,R>;192~WRITELN<R>'  '-4'   # runtime
t '2<N,0>;65<@0.0,@3.5,N>;2<R,0>;224~CEIL<N,R>;192~WRITELN<R>'   '-3'

# ----------------------------------------------------------------------
section "strings"
# ----------------------------------------------------------------------
t '2<STR, "hello">;2<UP, "">;208~UPPER<STR, UP>;192~WRITELN<UP>'   'HELLO'
t '2<R, "">;208~LOWER<"HELLO", R>;192~WRITELN<R>'                  'hello'
t '2<R, "">;208~CONCAT<"foo", "bar", R>;192~WRITELN<R>'            'foobar'
t '2<R, 0>;208~LEN<"hello", R>;192~WRITELN<R>'                     '5'
t '2<R, 0>;208~LEN<"", R>;192~WRITELN<R>'                          '0'
t '2<R, "">;208~SLICE<"hello", 1, 3, R>;192~WRITELN<R>'            'el'
t '2<R, "">;208~SLICE<"hello", 0, 5, R>;192~WRITELN<R>'            'hello'
t '2<R, 0>;208~FIND<"foobar", "bar", R>;192~WRITELN<R>'            '3'
t '2<R, 0>;208~FIND<"foobar", "xyz", R>;192~WRITELN<R>'            '-1'
t '2<STR, "strawberry">;2<COUNT, 0>;2<LEN, 0>;2<CH, "">;2<NEXT, 0>;208~LEN<STR, LEN>;32<I, 0, LEN, 1|64<I, 1, NEXT>;208~SLICE<STR, I, NEXT, CH>;112<80<CH, "r">|64<COUNT, 1, COUNT>>>;192~WRITELN<COUNT>' '3' 'count r in strawberry'

# ----------------------------------------------------------------------
section "library method matching (exact, not prefix)"
# ----------------------------------------------------------------------
t_err '192~W<"x">'                                     "IO: unknown method W"
t_err '2<R,0>;208~L<"hello",R>;192~WRITELN<R>'         "STRING: unknown method L"
t_err '2<R,0>;224~SQ<4,R>;192~WRITELN<R>'              "MATH: unknown method SQ"

# ----------------------------------------------------------------------
section "comparison"
# ----------------------------------------------------------------------
t '112<80<5,5>|192~WRITELN<"eq">>'                     'eq'
t '112<81<5,6>|192~WRITELN<"neq">>'                    'neq'
t '112<82<6,5>|192~WRITELN<"gt">>'                     'gt'
t '112<83<5,6>|192~WRITELN<"lt">>'                     'lt'
t '112<84<5,5>|192~WRITELN<"gte">>'                    'gte'
t '112<85<5,6>|192~WRITELN<"lte">>'                    'lte'
t '112<80<5,6>|192~WRITELN<"wrong">>'                  ''
t '112<80<"a","a">|192~WRITELN<"streq">>'              'streq'
t '112<82<9007199254740993,9007199254740992>|192~WRITELN<"gt">>'  'gt'   'int compare above 2^53'
t '112<80<1,@1.0>|192~WRITELN<"eq">>'                  'eq'              'int == float'

# ----------------------------------------------------------------------
section "logic and short-circuit"
# ----------------------------------------------------------------------
t '112<96<80<1,1>,80<2,2>>|192~WRITELN<"and">>'        'and'
t '112<96<80<1,1>,80<2,3>>|192~WRITELN<"and">>'        ''
t '112<97<80<1,2>,80<2,2>>|192~WRITELN<"or">>'         'or'
t '112<97<80<1,2>,80<2,3>>|192~WRITELN<"or">>'         ''
t '112<98<81<1,1>>|192~WRITELN<"not">>'                'not'
# F prints "side" when called; it only runs if the left side doesn't decide the result
t '16<F|192~WRITELN<"side">;18<1>>;112<96<81<1,1>,80<17<F>,1>>|192~WRITELN<"yes">>' ''              'AND: false LHS skips RHS'
t '16<F|192~WRITELN<"side">;18<1>>;112<97<80<1,1>,80<17<F>,1>>|192~WRITELN<"yes">>' 'yes'           'OR: true LHS skips RHS'
t '16<F|192~WRITELN<"side">;18<1>>;112<96<80<1,1>,80<17<F>,1>>|192~WRITELN<"yes">>' $'side\nyes'    'AND: true LHS runs RHS'
t '16<F|192~WRITELN<"side">;18<1>>;112<97<81<1,1>,80<17<F>,1>>|192~WRITELN<"yes">>' $'side\nyes'    'OR: false LHS runs RHS'

# ----------------------------------------------------------------------
section "branching: IF / ELIF / ELSE"
# ----------------------------------------------------------------------
t '2<n,1>;112<80<n,0>|65<n,1,n>>;113<80<n,1>|65<n,2,n>>;113<80<n,2>|65<n,3,n>>;114<192~WRITELN<"n must be 0-2">>;192~WRITELN<n>' '-1'
t '112<80<1,1>|192~WRITELN<"a">>;112<80<2,2>|192~WRITELN<"b">>'    $'a\nb'     'two separate IFs both run'
t '2<n,5>;112<80<n,0>|192~WRITELN<"zero">>;114|192~WRITELN<"nonzero">'   'nonzero'
t '2<n,0>;112<80<n,0>|192~WRITELN<"zero">>;114|192~WRITELN<"nonzero">'   'zero'
t '2<n,5>;112<80<n,0>|192~WRITELN<"zero">>;114|192~WRITELN<"nonzero">;192~WRITELN<"after">'  $'nonzero\nafter'  'ELSE body does not swallow the line'
t '2<n,2>;112<80<n,1>|192~WRITELN<"one">>;113<80<n,2>|192~WRITELN<"two">>;114|192~WRITELN<"other">'   'two'
t '2<n,9>;112<80<n,1>|192~WRITELN<"one">>;113<80<n,2>|192~WRITELN<"two">>;114|192~WRITELN<"other">'   'other'
t '2<n,1>;112<80<n,1>|192~WRITELN<"one">>;113<80<n,1>|192~WRITELN<"dup">>'                            'one'   'only the first matching branch runs'

# ----------------------------------------------------------------------
section "loops: FOR"
# ----------------------------------------------------------------------
t '32<I, 0, 5, 1|192~WRITELN<I>>'                          $'0\n1\n2\n3\n4'
t '2<SUM, 0>;32<I, 1, 11, 1|64<SUM, I, SUM>>;192~WRITELN<SUM>'  '55'
t '32<I,0,5,2|192~WRITELN<I>>'                             $'0\n2\n4'          'step > 1'
t '32<I,3,0,-1|192~WRITELN<I>>'                            $'3\n2\n1'          'literal negative step'
t '2<S,-1>;32<I,3,0,S|192~WRITELN<I>>'                     $'3\n2\n1'          'variable negative step'
t '2<S,1>;32<I,0,3,S|192~WRITELN<I>>'                      $'0\n1\n2'          'variable positive step'
t '32<I,5,0,1|192~WRITELN<I>>'                             ''                  'wrong direction: zero iterations'
t '32<I,0,0,1|192~WRITELN<I>>'                             ''                  'empty range'
t '2<N,3>;32<I,0,N,1|3<N,10>;192~WRITELN<I>>'              $'0\n1\n2'          'end is snapshotted'
t '2<S,0>;32<I,0,3,1|32<J,0,3,1|64<S,1,S>>>;192~WRITELN<S>'  '9'               'nested loops'
t_err '32<I,0,1,0|192~WRITELN<I>>'                         "FOR: literal zero step"
t_err '2<S,0>;32<I,0,5,S|192~WRITELN<I>>'                  "FOR: variable zero step"

# ----------------------------------------------------------------------
section "loops: WHILE and BREAK"
# ----------------------------------------------------------------------
t '2<I, 0>;2<S, 0>;33<83<I,5>|64<S,I,S>;64<I,1,I>>;192~WRITELN<S>'                        '10'
t '2<I, 0>;33<83<I,10>|112<80<I,5>|34<$0>>;64<I,1,I>>;192~WRITELN<I>'                     '5'
t '32<I,0,5,1|112<80<I,2>|34<$0>>;192~WRITELN<I>>'         $'0\n1'            'BREAK in FOR'
t '2<S,0>;32<I,0,3,1|32<J,0,3,1|112<80<J,1>|34<$0>>;64<S,1,S>>>;192~WRITELN<S>'           '3'   'BREAK exits inner loop only'
t_err '192~WRITELN<"a">;34<$0>;192~WRITELN<"b">'           "BREAK at top level"
t_err '34'                                                 "BREAK without args outside loop"
t_err '16<F|34<$0>>;32<I,0,3,1|17<F>>'                     "BREAK in function called from a loop"

# ----------------------------------------------------------------------
section "functions"
# ----------------------------------------------------------------------
t '2<RESULT, 0>;16<ADD_ONE, N|2<R, 0>;64<N, 1, R>;18<R>>;192~WRITELN<17<ADD_ONE, 5, RESULT>>'  '6'
t '16<SUM3, A, B, C|2<T, 0>;2<R, 0>;64<A, B, T>;64<T, C, R>;18<R>>;192~WRITELN<17<SUM3, 10, 20 , 30, OUT>>'  '60'
t '16<DOUBLE,X|2<R,0>;66<X,2,R>;18<R>>;16<QUAD,X|2<R,0>;17<DOUBLE,X,R>;17<DOUBLE,R,R>;18<R>>;192~WRITELN<17<QUAD,3,OUT>>'  '12'
t '16<FIB, N|112<85<N,1>|18<N>>;2<A,0>;2<B,0>;2<N1,0>;2<N2,0>;2<R,0>;65<N,1,N1>;65<N,2,N2>;17<FIB,N1,A>;17<FIB,N2,B>;64<A,B,R>;18<R>>;192~WRITELN<17<FIB,10>>'  '55'
t '16<FACT,N|112<85<N,1>|18<$1>>;2<A,0>;2<R,0>;65<N,1,A>;17<FACT,A,R>;66<N,R,R>;18<R>>;192~WRITELN<17<FACT,5>>'  '120'  'recursion: factorial'
t '192~WRITELN<17<F>>;16<F|18<7>>'                         '7'                 'call before definition'
t '16<F|2<X,1>>;192~WRITELN<17<F>>'                        'nil'               'function with no RET'
t '16<SHOW,V|192~WRITELN<V>>;17<SHOW,$7>'                  '7'                 'statement call, literal arg'
t '16<SHOW,V|192~WRITELN<V>>;2<X,7>;17<SHOW,X>'            '7'                 'statement call, variable arg (not DEST)'
t '2<X,1>;16<F|2<X,2>;192~WRITELN<X>>;17<F>;192~WRITELN<X>'  $'2\n1'           'local shadows global'
t_err '16<F|17<F>>;17<F>'                                  "infinite recursion -> clean error"
t_err '16<F|18<1>>;16<F|18<2>>'                            "duplicate function definition"
t_err '17<1>'                                              "call of a non-function"
t_err '2<X,99>;17<X>'                                      "out-of-range function index"

# ----------------------------------------------------------------------
section "scope and globals"
# ----------------------------------------------------------------------
t '2<n, 5>;16<inc_n|4<n>;64<n,1,n>>;17<inc_n>;192~WRITELN<n>'   '6'   'GLOBAL lets a function write a root var'
t '2<A,1>;2<B,2>;16<SWAP|4<A>;4<B>;2<T,0>;3<T,A>;3<A,B>;3<B,T>>;17<SWAP>;192~WRITELN<A>;192~WRITELN<B>'  $'2\n1'
t_err '2<n,5>;16<inc_n|64<n,1,n>>;17<inc_n>'               "undeclared variable inside function"

# ----------------------------------------------------------------------
section "arrays"
# ----------------------------------------------------------------------
t '2<ARR,48<10,20,30>>;2<V,0>;50<ARR,0,V>;192~WRITELN<V>'      '10'
t '2<ARR,48<10,20,30>>;2<V,0>;50<ARR,1,V>;192~WRITELN<V>'      '20'
t '2<ARR,48<10,20,30>>;2<V,0>;50<ARR,2,V>;192~WRITELN<V>'      '30'
t '2<ARR,48<1,2,3>>;51<ARR,0,99>;2<V,0>;50<ARR,0,V>;192~WRITELN<V>'   '99'
t '2<ARR,48<1,2,3>>;192~WRITELN<ARR>'                          '[1, 2, 3]'
t '2<ARR,48<5,3,8>>;112<49<3,ARR>|192~WRITELN<"found">>'       'found'
t '2<ARR,48<5,3,8>>;112<49<9,ARR>|192~WRITELN<"found">>;114|192~WRITELN<"not found">'  'not found'
t_err '2<ARR,48<1,2,3>>;2<V,0>;50<ARR,5,V>'                    "array index out of bounds (get)"
t_err '2<ARR,48<1,2,3>>;51<ARR,-1,0>'                          "array index out of bounds (set, negative)"
t_err '2<X,5>;2<V,0>;50<X,0,V>'                                "array op on a non-array"

# ----------------------------------------------------------------------
section "jumps and labels"
# ----------------------------------------------------------------------
t '128<SKIP>;192~WRITELN<"skipped">;144<SKIP>;192~WRITELN<"after">'                'after'
t '2<X,5>;129<X,5,HIT>;192~WRITELN<"miss">;144<HIT>;192~WRITELN<"hit">'            'hit'      'JEQ taken'
t '2<X,5>;129<X,6,HIT>;192~WRITELN<"miss">;144<HIT>;192~WRITELN<"hit">'            $'miss\nhit'  'JEQ not taken'
t '2<X,5>;130<X,6,HIT>;192~WRITELN<"miss">;144<HIT>;192~WRITELN<"hit">'            'hit'      'JNE taken'
t '131<6,5,HIT>;192~WRITELN<"miss">;144<HIT>;192~WRITELN<"hit">'                   'hit'      'JGT taken'
t '132<5,6,HIT>;192~WRITELN<"miss">;144<HIT>;192~WRITELN<"hit">'                   'hit'      'JLT taken'
t '2<I,0>;144<TOP>;192~WRITELN<I>;64<I,1,I>;132<I,3,TOP>'                          $'0\n1\n2'  'backward jump loop'
t '16<F|128<END>;192~WRITELN<"F-body">;144<END>;18<1>>;2<X,0>;128<END>;192~WRITELN<"root-skipped">;144<END>;17<F>;192~WRITELN<"done">'  'done'  'same label name in function and root'
t_err '128<MISSING>'                                       "undefined label at root"
t_err '16<F|128<NOWHERE>>;17<F>'                           "undefined label in function"

# ----------------------------------------------------------------------
section "io: stdin"
# ----------------------------------------------------------------------
t_in '2<X,"">;192~READ<X>;192~WRITELN<X>'                  'hello'  'hello'  'READ at root'
t_in '2<X,"">;16<F|192~READ<X>>;17<F>;192~WRITELN<X>'      'hi'     'hi'     'READ inside function'

# ----------------------------------------------------------------------
section "runtime errors (must exit non-zero, empty stdout)"
# ----------------------------------------------------------------------
t_err '2<R,0>;67<1,0,R>'                                   "division by zero (int)"
t_err '2<R,0>;67<@1.0,@0.0,R>'                             "division by zero (float)"
t_err '2<R,0>;68<5,0,R>'                                   "modulo by zero"
t_err '2<R,"">;208~SLICE<"hello",3,10,R>'                  "slice out of bounds"
t_err '2<R,0>;224~SQRT<R>'                                 "missing operand (only DEST given)"
t_err '18<1>'                                              "RET outside function"

# ----------------------------------------------------------------------
section "syntax errors (must exit non-zero, empty stdout)"
# ----------------------------------------------------------------------
t_err '114|'                                               "ELSE without a body"
t_err '2<n,5>;112<80<n,0>|192~WRITELN<"zero">>;114|192~WRITELN<"nonzero">>>'  "unmatched closer"
t_err '2<X'                                                "unterminated opcode"
t_err '999<1>'                                             "unknown opcode"

# ----------------------------------------------------------------------
section "design decisions"
# ----------------------------------------------------------------------
t '2<R,0>;224~POW<2,20,R>;192~WRITELN<R>'                  '1048576'    'integral float prints without exponent'
t '2<R,0>;224~POW<2,@0.5,R>;192~WRITELN<R>'                '1.4142135623731'  'POW with fractional exponent'
t_err '192~WRITELN<UNDECLARED>'                            "undeclared variable at root (interp: error, vm: nil)"

# ----------------------------------------------------------------------
section "stress (performance / limits)"
# ----------------------------------------------------------------------
t '16<FIB, N|112<85<N,1>|18<N>>;2<A,0>;2<B,0>;2<N1,0>;2<N2,0>;2<R,0>;65<N,1,N1>;65<N,2,N2>;17<FIB,N1,A>;17<FIB,N2,B>;64<A,B,R>;18<R>>;192~WRITELN<17<FIB,20>>'  '6765'  'FIB(20)'
t '2<S,0>;32<I,0,100000,1|64<S,I,S>>;192~WRITELN<S>'       '4999950000'  'sum 0..99999'
t '2<S,"">;2<N,0>;32<I,0,2000,1|208~CONCAT<S,"x",S>>;208~LEN<S,N>;192~WRITELN<N>'  '2000'  'repeated CONCAT'

# ======================================================================
# summary
# ======================================================================
echo
echo "--- PASSED: $passed - TOTAL: $total ---"
if (( ${#failures[@]} > 0 )); then
	echo -e "${RED}Failures:${NC}"
	for f in "${failures[@]}"; do echo "  $f"; done
	exit 1
fi
