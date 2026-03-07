# Changelog

All notable changes to this project will be documented in this file.

## [Unreleased]

### Fixed
- MeekRules R2/R3 now return immediately after first orientation, matching Java early-return semantics
- Collider orientation uses PRIORITIZE_EXISTING conflict rule instead of incorrect directed-path checks
- FAS `possibleParents` adds `isRequired` check alongside `isForbidden` for Knowledge support

### Added
- Full Knowledge class replacing stub: forbidden/required edge constraints, temporal tiers, tier-forbidden-within, edge iterators
- `pcOrientbk()` in PC: orients edges based on background knowledge before collider orientation
- MeekRules R4: orientation rule active when Knowledge is non-empty
- `colliderAllowed()` knowledge check in PC collider orientation
- `KnowledgeEdge` struct for edge enumeration
- Comprehensive Knowledge test suite (20 test cases)
- `TetradVersionRecommendation.md` documenting rationale for using Tetrad 7.6.8 as reference
- Target algorithm roadmap in CLAUDE.md (GFCI, BOSS, BOSS-FCI, GRASP, GRASP-FCI)

### Changed
- Reference Java source updated from 7.6.3 to 7.6.8 (critical FciOrient fixes for future FCI variants)
- Removed stale `tetrad` symlink; project now references `tetrad-7.6.8/`

## [0.1.0] - 2025-01-01

### Added
- Initial PC algorithm implementation (vertical slice)
- Fast Adjacency Search (FAS) with PC-Stable variant
- Meek Rules R1-R3 orientation propagation
- Fisher Z independence test via Cholesky decomposition
- Graph data structures: Node, Edge, Triple, EdgeListGraph
- ChoiceGenerator for C(n,k) enumeration
- SepsetMap for separation set tracking
- Knowledge stub (always permissive)
- DataSet wrapper around Eigen matrix with correlation computation
- Python bindings via nanobind (`run_pc_raw`, `PcResult`)
- Python facade class `TetradPort` with `run_pc()`, SEM helpers
- Catch2 test suite (183 assertions across 7 test files)
- CLI example `run_pc`
