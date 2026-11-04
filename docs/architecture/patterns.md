# Architecture Patterns

Patterns describe approaches we use repeatedly in this project. They explain
when an approach applies, the rules that matter, and its tradeoffs. A pattern
is not a rule for every situation, but differences should be intentional.

## Cooperative Tasks

Use a cooperative task for work that spans multiple frames. The caller starts
it once, then advances it from the frame callback until it is complete.
[TextureLoader](../../src/TextureLoader.h) is the current example.

The lifecycle uses `Start()`, `Update()`, and `IsPending()`:

- `Start()` sets up the work and returns a `Result`. Reject repeated starts
  before changing existing state.
- `Update()` advances the task on the caller's thread. Poll asynchronous work
  without waiting for it. Return control to the frame loop between updates.
- `IsPending()` reports whether more updates are needed. Call it after starting
  the task. Completion does not by itself mean that the work succeeded.

Track progress with an enum and a `switch` in `Update()`. The current loader
implements this lifecycle directly. There is no shared task base or batch
implementation in the repository.

When a task provides `Take()`, call it after `IsPending()` becomes false and
check its result. A successful take transfers ownership of the output and may
only happen once. Define how partial failures appear in that output. For
example, the texture loader preserves a null entry for each failed request.

Keep callback targets at stable addresses until callbacks finish or are
cancelled. A task that supports destruction while pending must cancel its
outstanding work before releasing callback targets. The texture loader does
this by closing its owned fetches when its requests are destroyed. Non-owned
dependencies must outlive all work and resources that use them.

## Valid Construction

An object should be valid and ready to use after it is constructed. Delete the
default constructor when default construction cannot produce a valid object.

When object creation can fail, instead of exposing a constructor, use a static
`Create()` function that returns a `Result<T>`. Keep the constructor private so
callers cannot bypass the creation path.

### General shape

```cpp
class Thing
{
public:
    Thing() = delete;

    static Result<Thing> Create(...);

private:
    Thing(...);
};
```
