#!/bin/bash

exe_path="./build/okin"

passed=0
total=0
use_vm=false

inputs=(
	"192~WRITELN<\"Hello, World!\">"
	"2<X, 10>;192~WRITELN<X>"
	"2<X, 10>;3<X, 20>;192~WRITELN<X>"
	"32<I, 0, 5, 1|192~WRITELN<I>>"
	"2<R, 0>;224~POW<2, 10, R>;192~WRITELN<R>"
	"2<STR, \"hello\">;2<UP, \"\">;208~UPPER<STR, UP>;192~WRITELN<UP>"
	"2<RESULT, 0>;16<ADD_ONE, N|2<R, 0>;64<N, 1, R>;18<R>>;192~WRITELN<17<ADD_ONE, 5, RESULT>>"
	"2<n, 5>;16<inc_n|4<n>;64<n,1,n>>;17<inc_n>;192~WRITELN<n>"
	"16<SUM3, A, B, C|2<T, 0>;2<R, 0>;64<A, B, T>;64<T, C, R>;18<R>>;192~WRITELN<17<SUM3, 10, 20 , 30, OUT>>"
	"2<n,1>;112<80<n,0>|65<n,1,n>>;113<80<n,1>|65<n,2,n>>;113<80<n,2>|65<n,3,n>>;114<192~WRITELN<\"n must be 0-2\">>;192~WRITELN<n>"
	"2<SUM, 0>;32<I, 1, 11, 1|64<SUM, I, SUM>>;192~WRITELN<SUM>"
	"2<STR, \"strawberry\">;2<COUNT, 0>;2<LEN, 0>;2<CH, \"\">;2<NEXT, 0>;208~LEN<STR, LEN>;32<I, 0, LEN, 1|64<I, 1, NEXT>;208~SLICE<STR, I, NEXT, CH>;112<80<CH, \"r\">|64<COUNT, 1, COUNT>>>;192~WRITELN<COUNT>"
	"16<FIB, N|112<85<N,1>|18<N>>;2<A,0>;2<B,0>;2<N1,0>;2<N2,0>;2<R,0>;65<N,1,N1>;65<N,2,N2>;17<FIB,N1,A>;17<FIB,N2,B>;64<A,B,R>;18<R>>;192~WRITELN<17<FIB,10>>"
	"2<n, 5>;16<inc_n|64<n,1,n>>;17<inc_n>;192~WRITELN<n>"
	"-- COMMENT"
)

expected=(
	"Hello, World!"
	"10"
	"20"
	$'0\n1\n2\n3\n4'
	"1024"
	"HELLO"
	"6"
	"6"
	"60"
	"-1"
	"55"
	"3"
	"55"
	""
	""
)

for ((i=0; i<${#inputs[@]}; i++)); do
	input="${inputs[i]}"
	exp="${expected[i]}"

	if [[ "$use_vm" == true ]]; then
		test_output=$($exe_path "$input" -vm)
	else
		test_output=$($exe_path "$input")
	fi

	if [[ "$test_output" == "$exp" ]]; then
		echo -e "\033[0;32m[PASS]\033[0m $input"
		((passed++))
	else
		echo -e "\033[0;31m[FAIL]\033[0m $input"
		echo "  expected: $exp"
		echo "  got:      $test_output"
	fi

	((total++))
done

echo "--- PASSED: $passed - TOTAL: $total ---"
