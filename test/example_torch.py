import torch
from pyco import co_yield,  coroutine

from torchvision.models import resnet18

# An iterable that returns each module that has
# just run with its inputs and outputs:
@coroutine
def forward_with_modules(net, *args, **kwargs):
    def fn(module, inputs, output):
        return co_yield((module, inputs, output))
    for m in net.modules():
        m.register_forward_hook(fn)
    return net(*args, **kwargs)

input = torch.randn(1, 3, 224, 224)
net = resnet18()
it = forward_with_modules(net, input)
for module, inputs, output in it:
    print("Module just ran", type(module))
    # resume_with will modify the output value of the network...
    # scale each  module output by 1.1
    it.resume_with(output*1.1)

    # e.g. maybe advance the FSDP buffer here.

output = it.result()


# Now lets make an iterable that returns each
# gradient as we compute it rather than all at the end
@coroutine
def grad(loss, parameters):
    # we return gradients in the same order as parameters
    # but autograd will return them in an arbitrary order
    # so we reorder
    grad_i = 0
    def visit(i, grad):
        nonlocal grad_i
        grad_p[i] = grad
        while grad_i < len(grad_p) and grad_p[grad_i] is not None:
            grad, grad_p[grad_i] = grad_p[grad_i], None
            co_yield(grad)
            grad_i += 1
    def register(i, p):
        return p.register_hook(lambda grad: visit(i, grad))
    to_remove = [register(i, p) for i, p in enumerate(parameters)]
    grad_p = [None for _ in to_remove]
    with torch.autograd.set_multithreading_enabled(False):
        loss.backward()
    for t in to_remove:
        t.remove()
    assert grad_i == len(to_remove)

# generally want the parameters backwards because
# the later ones are computed first
def reversed_parameters(net):
    return reversed(list(net.parameters()))

for grad in grad(output.sum(), reversed_parameters(net)):
    # e.g. Launch asynchronous communication here for DDP
    print(grad.sum())
