fuzz: fuzz.c
	$(CC) $(CFLAGS) -c fuzz.c -I ../../src/liblzma/api/
	$(CXX) $(CXXFLAGS) -lFuzzingEngine fuzz.o -o $(OUT)/fuzz \
	      ../../src/liblzma/.libs/liblzma.a

clean:
	rm -f *.o
