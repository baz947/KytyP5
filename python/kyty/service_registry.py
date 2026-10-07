from .compat import RuntimeError, Result

class ServiceRegistry:
    def __init__(self): self.t = {}
    def register(self, name, deps=()):
        if not name: return Result.fail(RuntimeError.InvalidArgument, "name")
        if name in self.t: return Result.fail(RuntimeError.InvalidArgument, "dup")
        self.t[name] = {"deps": list(deps)}
        return Result.ok()
    def check(self):
        for n, i in self.t.items():
            for d in i["deps"]:
                if d not in self.t:
                    return Result.fail(RuntimeError.MissingDependency, f"{n} needs {d}")
        return Result.ok()
    def lookup(self, n):
        if n not in self.t:
            return Result.fail(RuntimeError.MissingDependency, n)
        return Result.ok(self.t[n])
