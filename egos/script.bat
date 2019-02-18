echo run loops sequentially
loop 1000000000
loop 1000000000
echo run loops concurrently
loop 1000000000&
loop 1000000000&
wait
echo experiments done
