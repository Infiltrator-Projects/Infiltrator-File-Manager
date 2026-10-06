# Infiltrator File Manager

Infiltrator File Manager is a clean-sheet file-management project for InfiltratorOS.

The project is currently in the **research and architecture phase**. No production implementation has been selected yet.

Its goal is not to reproduce Nemo, Dolphin, Explorer, Finder or another existing file manager. Mature products, standards and current research are evidence. The project chooses the strongest justified mechanisms and owns its own interaction, operation and recovery semantics.

## Design position

Infiltrator File Manager is intended to complement InfiltratorFS while remaining a general-purpose file manager on other filesystems.

The key boundary is:

```text
filesystem exposes capability
        ↓
File Manager discovers and presents capability
        ↓
ordinary fallback remains available elsewhere
```

InfiltratorFS remains authoritative for its persistent format, transactions, allocation, integrity, history and storage policy. File Manager must not parse private InfiltratorFS structures or make ordinary operation depend on InfiltratorFS.

Where stable qualified interfaces exist, InfiltratorFS can enable stronger experiences such as persistent object identity, retained file history, reflink-aware copies, integrity information and richer recovery.

## Product principles

- first-principles engineering rather than competitor imitation;
- navigation, exact search and semantic/contextual retrieval as peer workflows;
- real files and the real filesystem namespace remain authoritative;
- semantic indexes are local, derived and rebuildable;
- deterministic file mutation with explicit failure and verification semantics;
- recovery/reversibility designed with the success path, not after it;
- capability discovery instead of filesystem-name assumptions;
- asynchronous first-paint-oriented UI behaviour;
- progressive disclosure: simple surface, deep capability;
- platform/toolkit details kept below a toolkit-neutral file/object model; and
- Mercedes-Benz-level technical excellence, reliability, effortless interaction and disciplined visual reduction as the execution quality bar.

## Current design documents

- [`docs/DESIGN.md`](docs/DESIGN.md) — product philosophy, InfiltratorFS relationship, quality bar and non-goals.
- [`docs/RESEARCH-2026.md`](docs/RESEARCH-2026.md) — current research review covering file retrieval, semantic systems, context, workflow, recoverability and interface design.
- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) — initial capability-based architecture and qualification model.

## Current research conclusion

The evidence reviewed so far does **not** support replacing folders with an AI interface.

It supports a stronger hybrid:

```text
navigation ─┐
exact search ├─→ authoritative file/object model → deterministic operations
semantic    ─┘                         ↓
                                  OS/filesystem
```

Context, semantic retrieval, activity views and related-item discovery can improve re-finding, but they remain derived overlays. The filesystem stays the source of truth.

Implementation should begin only after the remaining architecture questions and initial UI concepts have been prototyped and compared.
