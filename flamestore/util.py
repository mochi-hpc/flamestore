
def _hash(arr):
    x = 0x345678
    mult = 1000003
    m = 2**64
    length = len(arr)
    for y in arr:
        x = ((x ^ y) * mult)
        mult += (82520 + length * 2)
        x += 97531
        x = x % m
    return x


def _compute_signature(model, optimizer=None):
    arr = []
    for l in model.layers:
        for w in l.weights:
            arr.append(w.dtype.size)
            for d in w.shape:
                arr.append(int(d))
    if(optimizer is not None):
        for w in optimizer.weights:
            for d in w.shape:
                arr.append(int(d))
    return str(_hash(arr))
