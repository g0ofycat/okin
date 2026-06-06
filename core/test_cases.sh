#!/bin/bash

exe_path="./build/okin"

passed=0
total=0
use_vm=true

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

	"2<A, 10>;2<B, 3>;2<R, 0>;68<A, B, R>;192~WRITELN<R>"
	"2<A, 7>;2<B, 6>;2<R, 0>;66<A, B, R>;192~WRITELN<R>"
	"2<A, 0>;2<B, 5>;2<R, 0>;65<A, B, R>;192~WRITELN<R>"
	"2<A, 10>;2<B, 3>;2<R, 0>;67<A, B, R>;192~WRITELN<R>"
	"2<R, 0>;224~SQRT<144, R>;192~WRITELN<R>"
	"2<R, 0>;224~ABS<-42, R>;192~WRITELN<R>"
	"2<R, 0>;224~MIN<10, 3, R>;192~WRITELN<R>"
	"2<R, 0>;224~MAX<10, 3, R>;192~WRITELN<R>"
	"2<R, 0>;224~FLOOR<@3.9, R>;192~WRITELN<R>"
	"2<R, 0>;224~CEIL<@3.1, R>;192~WRITELN<R>"

	"2<R, \"\">;208~CONCAT<\"foo\", \"bar\", R>;192~WRITELN<R>"
	"2<R, 0>;208~LEN<\"hello\", R>;192~WRITELN<R>"
	"2<R, \"\">;208~SLICE<\"hello\", 1, 3, R>;192~WRITELN<R>"
	"2<R, \"\">;208~LOWER<\"HELLO\", R>;192~WRITELN<R>"
	"2<R, 0>;208~FIND<\"foobar\", \"bar\", R>;192~WRITELN<R>"
	"2<R, 0>;208~FIND<\"foobar\", \"xyz\", R>;192~WRITELN<R>"

	"112<80<5,5>|192~WRITELN<\"eq\">>"
	"112<81<5,6>|192~WRITELN<\"neq\">>"
	"112<82<6,5>|192~WRITELN<\"gt\">>"
	"112<83<5,6>|192~WRITELN<\"lt\">>"
	"112<84<5,5>|192~WRITELN<\"gte\">>"
	"112<85<5,6>|192~WRITELN<\"lte\">>"

	"112<96<80<1,1>,80<2,2>>|192~WRITELN<\"and\">>"
	"112<97<80<1,2>,80<2,2>>|192~WRITELN<\"or\">>"
	"112<98<81<1,1>>|192~WRITELN<\"not\">>"

	"2<I, 0>;2<S, 0>;33<83<I,5>|64<S,I,S>;64<I,1,I>>;192~WRITELN<S>"
	"2<I, 0>;33<83<I,10>|112<80<I,5>|34<$0>>;64<I,1,I>>;192~WRITELN<I>"

	"16<DOUBLE,X|2<R,0>;66<X,2,R>;18<R>>;16<QUAD,X|2<R,0>;17<DOUBLE,X,R>;17<DOUBLE,R,R>;18<R>>;192~WRITELN<17<QUAD,3,OUT>>"

	"2<A,1>;2<B,2>;16<SWAP|4<A>;4<B>;2<T,0>;3<T,A>;3<A,B>;3<B,T>>;17<SWAP>;192~WRITELN<A>;192~WRITELN<B>"

	"2<ARR,48<10,20,30>>;2<V,0>;50<ARR,1,V>;192~WRITELN<V>"
	"2<ARR,48<1,2,3>>;51<ARR,0,99>;2<V,0>;50<ARR,0,V>;192~WRITELN<V>"
	"2<ARR,48<5,3,8>>;112<49<3,ARR>|192~WRITELN<\"found\">>"
	"2<ARR,48<5,3,8>>;112<49<9,ARR>|192~WRITELN<\"found\">>;114<192~WRITELN<\"not found\">>"

	"128<SKIP>;192~WRITELN<\"skipped\">;144<SKIP>;192~WRITELN<\"after\">"
	"2<X,5>;129<X,5,HIT>;192~WRITELN<\"miss\">;144<HIT>;192~WRITELN<\"hit\">"
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

	"1"
	"42"
	"-5"
	"3"
	"12"
	"42"
	"3"
	"10"
	"3"
	"4"

	"foobar"
	"5"
	"el"
	"hello"
	"3"
	"-1"

	"eq"
	"neq"
	"gt"
	"lt"
	"gte"
	"lte"

	"and"
	"or"
	"not"

	"10"
	"5"

	"12"

	$'2\n1'

	"20"
	"99"
	"found"
	"not found"

	"after"
	"hit"
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
