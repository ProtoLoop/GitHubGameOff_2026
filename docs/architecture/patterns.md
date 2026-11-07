# Architecture Patterns

Patterns describe approaches we use repeatedly in this project. They explain
when an approach applies, the rules that matter, and its tradeoffs. A pattern
is not a rule for every situation, but differences should be intentional.

## Cooperative Tasks

Use a cooperative task when work must span multiple iterations of an event
loop without blocking it. The caller initiates the task, then advances it in
bounded steps until it is complete. This keeps the loop responsive, at the
cost of making intermediate states and their transitions explicit.

- Initiation establishes the task's state and reports failures to start.
  Reject unsupported repeated initiation before changing existing state.
- Advancement performs a bounded amount of work and polls asynchronous
  operations without waiting for them. Return control to the caller between
  steps.
- Pending status covers all unfinished processing, including work that
  remains after an asynchronous operation completes. Completion and success
  are separate facts.

Represent progress explicitly so each state has defined operations and
transitions. Status queries must remain safe after completion or failure.

If the task transfers ownership of its output, define when that transfer is
allowed and prevent repeated transfers. Report failures when retrieving the
output, and define whether partial failure produces usable output or fails
the whole task.

Keep callback targets alive and at stable addresses until callbacks finish
or are cancelled. Destruction must either cancel outstanding callbacks before
releasing their targets or leave those targets with an owner that outlives
the callbacks. Borrowed dependencies must outlive all work and resources that
use them.

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
