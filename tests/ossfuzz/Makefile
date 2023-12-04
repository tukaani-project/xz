FUZZ_TARGET_SRCS = $(wildcard *.c)
FUZZ_TARGET_BINS = $(FUZZ_TARGET_SRCS:.c=)

all: $(FUZZ_TARGET_BINS)

%: %.c
	$(CC) $(CFLAGS) -c $< -I ../../src/liblzma/api/ ;
	$(CXX) $(CXXFLAGS) $(LIB_FUZZING_ENGINE) $(<:.c=.o) -o $(OUT)/$@ \
		../../src/liblzma/.libs/liblzma.a ;

clean:
	rm -f *.o
