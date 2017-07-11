
LDLIBS= -lpthread

TESTS = \
	test-alloc \
	test-seq \
	test-seq-short \
	test-par-1prod \
	test-par-1prod-silent \
	test-par-1prod-check-order \
	test-par-2prod \
	test-par-16prod \
	test-par-16prod-bad \
	test-par-63prod \
	test-par-16prod-check-no-loss

all : tests

tests : ${TESTS}

test-alloc : test-alloc.o hwfq.o

test-seq : test-seq.o hwfq.o

test-seq-short : test-seq-short.o hwfq.o

test-par-1prod : test-par-1prod.o hwfq.o

test-par-1prod-silent : test-par-1prod-silent.o hwfq.o

test-par-1prod-check-order : test-par-1prod-check-order.o hwfq.o

test-par-2prod : test-par-2prod.o hwfq.o

test-par-16prod : test-par-16prod.o hwfq.o

test-par-16prod-bad : test-par-16prod-bad.o hwfq.o

test-par-63prod : test-par-63prod.o hwfq.o

test-par-16prod-check-no-loss : test-par-16prod-check-no-loss.o hwfq.o

clean :
	rm -rf *.o ${TESTS}
