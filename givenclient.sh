rm stest btest ctest
./client -h 143.248.53.25 -p 7878 -o 0 -k abcd < testvectors/crossbow.txt > ctest
./client -h 143.248.53.25 -p 7878 -o 0 -k abcd < testvectors/sample.txt > stest
./client -h 143.248.53.25 -p 7878 -o 0 -k abcd < testvectors/burrito.txt > btest
diff testvectors/crossbow_encrypted.txt ctest
diff testvectors/burrito_encrypted.txt btest
diff testvectors/sample_encrypted.txt stest
./client -h 127.0.0.1 -p 7878 -o 0 -k abcd < testvectors/crossbow.txt > ctest2
./client -h 127.0.0.1 -p 7878 -o 0 -k abcd < testvectors/sample.txt > stest2
./client -h 127.0.0.1 -p 7878 -o 0 -k abcd < testvectors/burrito.txt > btest2
diff testvectors/crossbow_encrypted.txt ctest2
diff testvectors/burrito_encrypted.txt btest2
diff testvectors/sample_encrypted.txt stest2
# rm ctest* btest* stest*