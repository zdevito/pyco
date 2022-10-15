import time
from pyco import co_yield, coroutine
from pyco.magic_trace import magic_trace

def totime(n):
    for i in range(n):
        yield i

@coroutine
def totimeco(n):
    for i in range(n):
        co_yield(i)


t0 = time.time()

# if True:
with magic_trace('trace_gen.fxt'):
    i = 0
    for _ in totime(1000000):
        i += 1
t1 = time.time()

# if True:
with magic_trace('trace_co.fxt'):
    j = 0
    for _ in totimeco(1000000):
        j += 1

t2 = time.time()
assert i == j
print("Overhead compared to simple generator ", (t2 - t1) / (t1 - t0))
