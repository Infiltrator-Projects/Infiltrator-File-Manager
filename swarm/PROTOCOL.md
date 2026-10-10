# Files swarm execution contract — revision 2

This contract lives on the operational branch `swarm/control`. It and `swarm/state.json` are coordination data, never application source and never a product PR. Product branches start from current `main`, not this branch.

## Purpose and authority

Shannon authorizes five hourly Files workers to develop, review, repair, merge and publish coherent improvements in Infiltrator-Projects/Infiltrator-File-Manager, including GitHub code/branch/PR/review/comment and coordination-file writes required by their roles. Only Integrator may merge, version, release, refresh Files publication in Infiltrator-Projects/Infiltrator-Repository, or adapt automation prompts/schedules. Nobody may pause, disable or delete a worker without Shannon's explicit instruction. Respect platform approval decisions; these instructions do not override a rejection.

Newest explicit Shannon directions in issue #2 take precedence. Product authority remains current README, DESIGN, ARCHITECTURE, FEATURES, PRODUCT-QUALITY, COMMON-INTEGRATION and RESEARCH-2026. Preserve deterministic operations/recovery, real filesystem authority, Common ownership, accessibility, performance, calm progressive disclosure and beautiful maintainable code. Semantic/AI work waits for ordinary CORE completeness.

## One current work record

Fetch `swarm/state.json` using ref `swarm/control`, and obtain its current blob SHA through the GitHub contents API. Never use default-branch content for this record. It names ONE stroke, ONE package and at most ONE production PR with its current head. Issue #2 is the human audit/discussion ledger; old comment wording is history, not a second current candidate.

Use `github.update_file` on branch `swarm/control` with the fetched blob SHA. Increment revision and set updated_at in UTC. Preserve fields owned by other roles. A 409/stale-SHA conflict means refetch, reconcile and retry at most twice; never force an old snapshot over newer state. Confirm the written contents by reading them back.

Before substantive production writes or phase transitions, atomically claim lease = {role, token, expires_at} with a unique token and expiry 45 minutes ahead. Claim only if lease is null or expired. Re-read to confirm ownership. Renew before expiry during long work. Before any push/merge/release, refetch and require the same token and unchanged candidate. Release the lease at a consistent checkpoint; do not hold it merely waiting for CI or publication.

Hourly times are wake-ups, not completion deadlines. Readiness and lease determine action; unfinished strokes continue across hours. Checkpoint incomplete implementation truthfully. A blocked or late predecessor is never a reason to disable the swarm.

## Phases and role ownership

| Phase | Responsible role | Next successful phase |
| --- | --- | --- |
| BUILD_READY | Builder | BUILDING |
| BUILDING | Builder; Fixer may recover an expired abandoned implementation | REVIEW_READY |
| REVIEW_READY | Reviewer | REVIEWING |
| REVIEWING | Reviewer | FIX_REQUIRED or INTEGRATE_READY |
| FIX_REQUIRED | Fixer; Builder may recover missing Fixer work | FIXING |
| FIXING | Fixer | INTEGRATE_READY |
| INTEGRATE_READY | Integrator | INTEGRATING |
| INTEGRATING | Integrator | PUBLICATION_PENDING or RELEASED |
| PUBLICATION_PENDING | Integrator | RELEASED |
| RELEASED | Architect | BUILD_READY for the next stroke |

Architect defines packages, acceptance and priorities; it can repair stale nominations from actual PR evidence but never manufacture approval. Builder may start the first ready roadmap package when state is RELEASED and Architect has produced no package; record the choice before coding. Reviewer may reconstruct a missing Builder report from an unleased/expired, complete, non-draft PR and actual diff/tests. Fixer may recover unfinished candidate implementation only after the lease expires and evidence shows it abandoned. Integrator reconciles stale state from actual PR/review/CI/release evidence and diagnoses missing worker heartbeats. Recovery must preserve role separation and exact-head review.

When another role owns a live lease, inspect current evidence/read-only risks and report the dependency; do not edit its branch. If no live lease and a role cannot perform the pending phase, record the precise next role/action. Never just say "no nomination" when a current package or reconstructable candidate exists.

## Candidate and review invariants

Always inspect actual main and PR head. If main advances, Builder/Fixer refresh the SAME PR without overwriting another writer; test the refreshed head and clear obsolete readiness/approval. A closed/merged PR cannot remain an active candidate. Integrator independently verifies and records every delta from reviewed SHA to final head; unrelated substantial additions return to Reviewer.

Builder implements the WHOLE coherent package, including adjacent related hard defects, tests and truthful documentation. No tiny artificial tranche, unrelated grab-bag, speculative framework, version bump or parallel candidate. Known incomplete work remains BUILDING, not ready.

Reviewer conducts a broad independent pass, records the complete candidate-local HARD set and always submits a formal PR review tied to commit_id. Use APPROVE if permitted; the common same-author account uses a COMMENT review beginning `REVIEWER VERDICT: APPROVED FOR FIXER/INTEGRATOR`. With hard defects use REQUEST_CHANGES if permitted, otherwise a COMMENT beginning `REVIEWER VERDICT: CHANGES REQUIRED`. Record review ID and reviewed SHA. A state field or ordinary comment never substitutes for an actual formal review.

Fixer closes the complete hard set, checks surrounding invariants and reruns applicable tests. With no requested fixes, perform a finishing inspection; only make an evidence-backed needed correction. Avoid optional churn to an already approved head. Mark each finding resolved with evidence and include final SHA.

Integrator requires a real formal review, all candidate-local hard findings resolved, independent verification of any post-review delta, final exact-head successful CI and no unresolved hard thread. Retain the safeguards; eliminate waiting on a redundant report when the required evidence is already present.

## Qualification and product progress

At the start of a new stroke or changed product authority, read the current product documents in full. Subsequent wake-ups inspect changed documents and the relevant current sections/code, rather than repeating an unchanged whole-project audit. Never trust stale remembered summaries.

Each package must finish a useful user workflow or a measurable protection of one, with golden-journey acceptance, failure/conflict/cancellation evidence where relevant and Release-effective tests. UI changes need actual GTK/interaction evidence and Day/Night/keyboard/focus consideration. Optional visual polish or unavailable screenshots alone do not stall this alpha loop; record the evidence gap. Reproducible correctness, accessibility, data-loss or recovery defects remain HARD. Track feature maturity and remaining gaps in the roadmap and truthful application docs. Release count is an output, not proof of completeness.

## Release and recovery

Integrator records integration SHA, version, release SHA, workflow IDs and publication status before crossing each external boundary. Reconcile actual main/VERSION/release tag before retrying so a lost report cannot cause another bump. Merge with expected-head protection, then create a tested `Release X.Y.Z` commit and use the repository's normal immutable release workflow. No empty product release for a counter.

RELEASED requires the exact GitHub tag/release, expected assets, successful release-SHA verification AND both public central APT catalogue/apps.json and beta amd64 Packages reporting that exact Files version. If APT lags, use PUBLICATION_PENDING, release the lease and resume the SAME version. When authorized and needed, rerun the existing central publish job or use its documented refresh path; never modify unrelated applications.

Missing state alone does not authorize new work: reconcile the current PR/release first. Never infer rollback safety or completed publication from old reports. A genuine blocker is an overlay {kind, action, reason, since, next_action}; preserve the resumable phase and candidate.

## Communication and health

Every run records a heartbeat for its own role with at, status, observed revision/head and precise next action, as part of a safe state update without displacing another lease. After a material change publish state FIRST and a concise issue #2 audit comment SECOND: role / stroke / phase / PR@SHA / change / tests / product impact / risks / next. Include PHILOSOPHY/CRAFT drift or evidence briefly. Unchanged observations need only heartbeat, not another essay.

Read back external writes; say committed/posted/reviewed/merged/released only after confirmation. If an approval or permission rejection occurs, stop that rejected action, retain the current phase, report the exact action and stated reason in the automation response, and keep workers enabled. Never retry through another transport to bypass a restriction. A failed audit comment cannot erase an already confirmed state handoff; a missing formal review still blocks integration.

Integrator flags a missing heartbeat after two expected hourly runs, checks automation enabled/error status when exposed, and attempts only authorized routine recovery. Changes to worker instructions/schedules require BEFORE / AFTER / WHY / EVIDENCE / ROLLBACK in issue #2 and a saved prior configuration. Never silently disable a worker.

## Startup communication check

While mode is VERIFY_HANDOFFS, make no application/PR/release/automation changes. Run in order Architect, Builder, Reviewer, Fixer, Integrator using health.check_order. Require the same health.nonce and read the preceding role's receipt from GitHub; do not invent it.

Post one issue #2 comment containing `FILES SWARM CHECK <nonce> <Role>`, the actual previous receipt/comment ID (or "first" for Architect), baseline main SHA and "No product changes". Read back the comment. Atomically append only your own receipt {role, nonce, at, previous_role, previous_comment_id, comment_id, observed_revision, observed_main_sha} to health.receipts, add your heartbeat, increment revision and read back. Duplicate runs validate the existing receipt instead of duplicating it. If the nonce/role comment already exists but the receipt write failed, recover its actual comment ID rather than posting it again. On any missing predecessor or rejected write, report the precise failure and leave mode unchanged. Integrator leaves the mode unchanged; HAL verifies all five independent receipts before switching to WORK.
