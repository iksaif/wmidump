PROG=	./../wmidump

GOOD=	good-01.in good-02.in good-03.in good-04.in
BAD=	bad-01.in bad-02.in bad-03.in bad-04.in bad-05.in

# Test environment, enable malloc.conf(5) security options on OpenBSD.
TENV=	MALLOC_OPTIONS=S

all:
	@(set -e; \
	  for t in ${GOOD} ${BAD}; do \
	    b=$${t%.in}; \
	    ${PROG} <$$t >$$b.out 2>&1 || true; \
	    if env ${TENV} diff -au $$b.ok $$b.out; then \
	      echo PASS: $$t; \
	    else \
	      echo FAIL: $$t; \
	      exit 1; \
	    fi; \
	  done; \
	  exit 0)

clean:
	rm -f ${GOOD:.in=.out} ${BAD:.in=.out}

.PHONY: clean
