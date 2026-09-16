# Code Review Checklist

Checklist applied to every change in this repo before merge, focused on the
categories of bugs that are specific to multi-threaded C++ systems code.

## Concurrency correctness

- [ ] Every piece of shared mutable state is protected by a specific,
      identifiable mutex (or is a documented atomic with a stated reason
      why a plain atomic is sufficient).
- [ ] No lock is held across a blocking call to another component (risk of
      deadlock / priority inversion).
- [ ] Every `wait()` on a `condition_variable` uses a predicate (spurious
      wakeup safe) rather than a bare wait.
- [ ] Shutdown path is reviewed explicitly: can every blocked thread be
      woken and exit cleanly? (`close()` + `notify_all()` on both CVs.)
- [ ] New code has been run at least once under ThreadSanitizer
      (`-DENABLE_TSAN=ON`) before merge, not just under the default build.

## Resource ownership & lifetime

- [ ] No raw `new`/`delete`; ownership is expressed via RAII, references,
      or (if genuinely needed) smart pointers.
- [ ] Every `std::thread` has an unambiguous single owner responsible for
      `join()`; no detached threads unless explicitly justified in a
      comment.
- [ ] Destructors are safe to call even if `start()`/`stop()` were never
      called, or `stop()` was called twice.

## Interface & design

- [ ] Public methods have a one-line comment stating blocking vs.
      non-blocking behavior explicitly (this bit us conceptually while
      designing `push()` vs `try_push()` — now a hard rule).
- [ ] New virtual methods on `Node` are justified against the existing
      `process()` hook before adding a second one — prefer composition
      over widening the base class interface.

## Tests

- [ ] Every new class of bug fixed gets a regression test, not just a fix.
- [ ] Concurrency-sensitive code has at least one test that exercises the
      blocking path (not just the single-threaded happy path) — see
      `BoundedQueue.BlockedPushUnblocksWhenConsumerPops` for the pattern.

## Style / readability

- [ ] Builds warning-clean under `-Wall -Wextra -Wpedantic`.
- [ ] No behavior change hidden inside an unrelated formatting/rename diff.
