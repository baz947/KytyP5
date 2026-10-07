from .compat import RuntimeError, Result

class SaveDataManager:
    def __init__(self):
        self.committed = {}; self.staged = {}
    def mount(self, user, title, quota=64*1024*1024):
        if not user or not title:
            return Result.fail(RuntimeError.InvalidArgument, "user/title")
        return Result.ok({"user": user, "title": title, "quota": quota})
    @staticmethod
    def _key(m, rel): return f"{m['user']}|{m['title']}|{rel}"
    def _usage(self, m):
        scope = f"{m['user']}|{m['title']}|"
        return sum(len(v) for k, v in {**self.committed, **self.staged}.items()
                   if k.startswith(scope))
    def stage(self, m, rel, data: bytes):
        if not rel or rel.startswith("/") or ".." in rel:
            return Result.fail(RuntimeError.InvalidArgument, "rel")
        if self._usage(m) + len(data) > m["quota"]:
            return Result.fail(RuntimeError.HostMemoryFailure, "quota")
        self.staged[self._key(m, rel)] = bytes(data)
        return Result.ok()
    def read(self, m, rel):
        k = self._key(m, rel)
        if k in self.staged: return Result.ok(self.staged[k])
        if k in self.committed: return Result.ok(self.committed[k])
        return Result.fail(RuntimeError.InvalidArgument, "no save")
    def commit(self, m):
        scope = f"{m['user']}|{m['title']}|"
        for k, v in list(self.staged.items()):
            if k.startswith(scope):
                self.committed[k] = v; del self.staged[k]
        return Result.ok()
    def rollback(self, m):
        scope = f"{m['user']}|{m['title']}|"
        for k in [k for k in self.staged if k.startswith(scope)]:
            del self.staged[k]
        return Result.ok()
