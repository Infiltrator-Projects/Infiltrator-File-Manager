# Files delivery queue

Baseline: Files 0.1.16, main 8c2daf6cdf3e214186b9fdb880799ad8ae5c554f. The full intended scope remains docs/FEATURES.md; these are actionable near-term packages, not a replacement product design.

1. **Destination ownership under competing writers — ready, highest priority.** Protect Copy/Move/Rename/Keep Both from silently replacing another writer's destination; prove ownership before recursive cleanup; protect replacement rollback from deleting a destination changed by another actor. GJ-05/06/07/08. Reproduce with deterministic interleavings and exercise failure/cancellation paths in Release. Preserve intentional Replace semantics and honest recovery.
2. **Journal durability and live-session ownership — next.** Persist newly created journal namespace entries, handle EINTR, serialize complete records and distinguish live operations from previous-session interruptions. Crash/concurrent writer and second-window fixtures; use qualified Common primitives. GJ-06/08.
3. **Operation window disposal — next.** Close a window during progress/dialog/chooser completion without accessing disposed children. Hosted GTK regressions and clear cancellation/result ownership. GJ-01/06/11.
4. **Complete ordinary clipboard workflow — after the relevant safety foundations.** Cut/copy/paste, keyboard/context actions, shared operation/conflict/progress/recovery path, unambiguous cut state and cancellation. Qualify the entire workflow rather than adding disconnected buttons. GJ-05/06/07/11.
5. **File/object/capability foundation and useful properties — sequence from current architecture.** Honest metadata/permissions/capabilities, asynchronous presentation, explicit unavailable states; no filesystem-name assumptions or UI/core leakage.
6. **Navigation completeness.** Bookmarks and tabs in coherent complete packages, preserving history/selection/context, keyboard operation and calm composition. GJ-02/11.

Architect may refine the next package from current source and user priorities. Finish or explicitly resolve the active stroke before replacing it. Track completed package, evidence, released version and remaining user capability after each release. Do not claim CORE completeness, remote/provider, performance or accessibility qualification without evidence.

The source-review risks above were recorded in issue #2 comment 6096430693. They are findings requiring fixtures and fixes, not proof that data loss has occurred.
