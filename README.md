# Infiltrator File Manager

Infiltrator File Manager is a clean-sheet file-management project for InfiltratorOS.

**Current source version:** 0.1.0  
**Shared foundation:** exact Infiltratr Common 1.19.38 gitlink, linked through the full Common target on InfiltratorOS/POSIX  
**Desktop implementation:** C++20 + GTK 4/GIO  
**Status:** early implementation; read-only browsing tranche

The project does not exist to reproduce Nemo, Dolphin, Explorer, Finder or another existing file manager. Mature products, standards and current research are evidence. The project chooses the strongest justified mechanisms and owns its own interaction, operation and recovery semantics.

The implementation has now started, but the architecture remains deliberately capable of changing while the early model and interaction contracts are being proven. The first tranche establishes a real, buildable browser without prematurely adding mutation, semantic indexing or filesystem-specific policy.

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
- platform/toolkit details kept below a toolkit-neutral file/object model;
- Common is the canonical first-party substrate for strong product-neutral mechanics rather than a token dependency; and
- Mercedes-Benz-level technical excellence, reliability, effortless interaction and disciplined visual reduction as the execution quality bar.

## Complete intended product surface

The full feature catalogue is maintained in [`docs/FEATURES.md`](docs/FEATURES.md). It covers both mature file-manager parity and the capabilities that make Files an Infiltrator product: deterministic recoverable operations, exact/content/semantic retrieval, contextual activity, first-class InfiltratorFS identity/history/reflinks/integrity, remote/device/archive handling, security, accessibility, performance and qualification.

The catalogue is intentionally broader than the current release. It is the product contract used to plan implementation and to prevent local UI changes from silently becoming architecture.

Common's role is documented separately in [`docs/COMMON-INTEGRATION.md`](docs/COMMON-INTEGRATION.md). That ledger identifies which mechanics Files should inherit from Common, which semantics remain Files-owned, and which capabilities remain InfiltratorFS-owned.

## Current 0.1.0 implementation

The initial executable is `infiltrator-file-manager`, presented to the user as **Files**.

It currently provides:

- Home, Desktop, Documents, Downloads, Computer and Trash entry points;
- Back, Forward and Up navigation;
- an editable location field accepting paths and URIs;
- asynchronous, monitored directory enumeration through GTK/GIO;
- a virtualised GTK 4 list presentation rather than one widget per directory entry;
- native file/folder icons, file sizes and item count/status;
- directory activation and launching files through the platform's registered default application;
- a toolkit-neutral `Location` model and navigation history beneath the GTK presentation;
- the canonical Infiltrator Day/Night/System design contract from pinned Common;
- the full `InfiltratrCommon::Common` build dependency on InfiltratorOS/POSIX, making Common's POSIX/state/I/O contracts available to the non-UI layers as they are introduced; and
- a hosted build/test gate on every main-branch update.

The current tranche deliberately has **no copy, move, rename, delete, overwrite or create operations**. Mutation will not be added ad hoc to UI callbacks. Those operations first require the deterministic operation engine described in the architecture: preflight, plan, execute, durability, verification and recoverable result state.

Semantic/context indexing and InfiltratorFS-specific capability providers are also deliberately absent from this first executable. Ordinary file-manager correctness comes first.

## Build

On a Debian-family development system with GTK 4 development files installed:

```bash
git clone --recurse-submodules https://github.com/Infiltrator-Projects/Infiltrator-File-Manager.git
cd Infiltrator-File-Manager
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/infiltrator-file-manager
```

The build rejects a missing, wrong-version or wrong-commit Infiltratr Common checkout. Version 0.1.0 is pinned to Common 1.19.38 at commit `04b5e219924ec0e65ef9d254c114fad4de0abd29`.

## Design documents

- [`docs/FEATURES.md`](docs/FEATURES.md) — complete intended feature catalogue and release sequencing.
- [`docs/COMMON-INTEGRATION.md`](docs/COMMON-INTEGRATION.md) — Common ownership and integration ledger.
- [`docs/DESIGN.md`](docs/DESIGN.md) — product philosophy, InfiltratorFS relationship, quality bar and non-goals.
- [`docs/RESEARCH-2026.md`](docs/RESEARCH-2026.md) — research review covering file retrieval, semantic systems, context, workflow, recoverability and interface design.
- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) — capability-based architecture, deterministic operation boundary and qualification model.

## Research conclusion

The evidence reviewed so far does **not** support replacing folders with an AI interface.

It supports a stronger hybrid:

```text
navigation ─┐
exact search ├─→ authoritative file/object model → deterministic operations
semantic    ─┘                         ↓
                                  OS/filesystem
```

Context, semantic retrieval, activity views and related-item discovery can improve re-finding, but they remain derived overlays. The filesystem stays the source of truth.

## Implementation sequence

The implementation proceeds by proving boundaries rather than accumulating visible features:

1. read-only navigation shell and toolkit-neutral location model, with Common used for every applicable product-neutral contract;
2. deterministic mutation/operation engine with explicit failure and verification state;
3. richer file/object/capability model and volume/remote-location handling;
4. mature parity features such as tabs, split view, bookmarks, properties, permissions, thumbnails/previews, archives and batch rename;
5. exact/content search and rebuildable indexing;
6. qualified InfiltratorFS capability provider for persistent identity, native history, reflinks, integrity and stronger recovery;
7. contextual/semantic retrieval as a rebuildable non-authoritative layer; and
8. continued accessibility, performance, visual and interaction refinement against the Common/Infiltrator design contract.

Each tranche must remain useful and testable without making later architectural layers authoritative by accident.
