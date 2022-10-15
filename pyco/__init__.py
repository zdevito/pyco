from _pyco import co_yield, Coroutine

# decorator
def coroutine(fn):
    def _fn(*args, **kwargs):
        return Coroutine(lambda: fn(*args, **kwargs))
    return _fn
