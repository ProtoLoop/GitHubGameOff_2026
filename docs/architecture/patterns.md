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

## Owned and borrowed handles

An owned handle releases its node through its owner when destroyed. Moving it
transfers ownership and leaves the source empty. Move assignment releases the
destination's node before taking ownership, except for self-assignment.
Explicit early release through `Level` clears the handle so its destructor
does not release the node again.

The owner must remain at a stable address and outlive its active owned handles.
`Level` cannot be copied or moved. `Game` owns it through `std::unique_ptr` and
destroys its entities before destroying the level. Dependent nodes must be
released before the nodes they borrow.

Use `.Borrow()` to obtain access without transferring ownership. Each non-empty
borrowed handle contributes to the slot's borrow count. Copying adds a borrow,
moving transfers it, and destruction releases it. Moving leaves the source empty.

All borrows must be released before destroying the referenced node. Merely
stopping access is insufficient because destroying a borrowed handle accesses
its slot. The slot's owner must remain alive through that destruction. Borrow
counting diagnoses lifetime violations. It does not keep nodes alive or defer
their destruction.

See the handle types in [Handle.h](../../src/Handle.h), their node aliases in
[Nodes.h](../../src/Nodes.h), and the
[ownership decision](decisions.md#raii-node-handles-and-stable-level-ownership).

## Parent transforms

A `Node2D` may borrow a parent node. Its stored transform, position, rotation,
direction getters, and movement methods use the parent's coordinate space.
Without a parent, that space is world space. `GetWorldTransform()` composes
ancestor transforms with the node's local transform.

Parent links affect transforms but do not transfer ownership or cascade
destruction. Destroy children before their parents. Release other outstanding
borrows before destroying each node. Destroying a child releases its stored
borrow of the parent.

See [Node2D](../../src/Nodes.h).

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
