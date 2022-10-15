from pyco import co_yield, coroutine



# 0 - 1 - 3 - 4
#  \- 2 /
example_graph = [
    (0, 1),
    (1, 3),
    (3, 4),
    (0, 2),
    (2, 3)
]

@coroutine
def dfs(graph):
    seen = [False for _ in graph]
    def visit(n):
        co_yield(('pre', n))
        if seen[n]:
            return
        seen[n] = True
        for f, t in graph:
            if f == n:
                visit(t)
        co_yield(('post', n))
    visit(0)




for n in dfs(example_graph):
    print(n)

# ('pre', 0)
# ('pre', 1)
# ('pre', 3)
# ('pre', 4)
# ('post', 4)
# ('post', 3)
# ('post', 1)
# ('pre', 2)
# ('pre', 3)
# ('post', 2)
# ('post', 0)
