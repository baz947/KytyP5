from .compat import RuntimeError, Result

def _norm(p):
    if not p or not p.startswith("/"):
        return Result.fail(RuntimeError.InvalidArgument, "absolute required")
    if "\\" in p:
        return Result.fail(RuntimeError.InvalidArgument, "host sep")
    parts = []
    for cur in p[1:].split("/"):
        if cur in ("", "."): continue
        if cur == "..":
            if not parts:
                return Result.fail(RuntimeError.InvalidArgument, "escapes root")
            parts.pop()
        else: parts.append(cur)
    return Result.ok("/" + "/".join(parts))

class VirtualFs:
    def __init__(self):
        self.files = {}; self.dirs = {"/app0": True}
    def write(self, p, data: bytes):
        n = _norm(p)
        if not n.is_ok(): return n
        self.files[n.value] = bytes(data)
        return Result.ok()
    def read(self, p):
        n = _norm(p)
        if not n.is_ok(): return n
        if n.value not in self.files:
            return Result.fail(RuntimeError.InvalidArgument, "no file")
        return Result.ok(self.files[n.value])
    def mkdir(self, p):
        n = _norm(p)
        if not n.is_ok(): return n
        self.dirs[n.value] = True
        return Result.ok()
    def stat(self, p):
        n = _norm(p)
        if not n.is_ok(): return n
        if n.value in self.files:
            return Result.ok({"dir": False, "size": len(self.files[n.value])})
        if n.value in self.dirs:
            return Result.ok({"dir": True})
        return Result.fail(RuntimeError.InvalidArgument, "no entry")
    def listdir(self, p):
        n = _norm(p)
        if not n.is_ok(): return n
        prefix = "/" if n.value == "/" else n.value + "/"
        out = [k[len(prefix):] for k in self.files if k.startswith(prefix)
               and "/" not in k[len(prefix):]]
        return Result.ok(out)
