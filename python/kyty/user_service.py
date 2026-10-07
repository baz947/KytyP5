from .compat import RuntimeError, Result

class UserService:
    def __init__(self):
        self._next = 10000000; self.users = {}; self.active = 0
    def create(self, name):
        if not name:
            return Result.fail(RuntimeError.InvalidArgument, "name")
        i = self._next; self._next += 1
        self.users[i] = name
        if not self.active: self.active = i
        return Result.ok(i)
    def set_active(self, i):
        if i not in self.users:
            return Result.fail(RuntimeError.InvalidArgument, "user")
        self.active = i
        return Result.ok()
    def active_user(self):
        if self.active not in self.users:
            return Result.fail(RuntimeError.MissingDependency, "no active")
        return Result.ok((self.active, self.users[self.active]))
