# CRAN Package Plan: rtetrad

R interface to tetrad-port via Rcpp + RcppEigen, using a monorepo layout with vendoring at release time.

## Strategy

- R package lives at `r/rtetrad/` in this repo
- During development, a `configure` script creates symlinks from `src/tetrad/` → canonical C++ source
- `Makevars` compiles using local `tetrad/` paths (works with both symlinks and vendored copies)
- For CRAN submission, a script vendors (copies) all C++ source into `src/tetrad/`
- One C++ codebase, always — vendoring is a release step, not a development workflow

## Prerequisites: C++ Changes Required

### 1. stdout/stderr Redirect (CRAN-blocking)

CRAN forbids `std::cout` and `std::cerr`. The C++ core currently uses `std::cout` for all verbose output (PC, FAS, FGES, MeekRules, FciOrient).

**Solution:** Refactor verbose output to use a configurable `std::ostream*`. This is a small change that benefits all consumers (Python can suppress output too).

```cpp
// Add to a new src/util/log_stream.h
namespace tetrad {
    // Default: std::cout. R sets to Rcpp::Rcout. Tests can use a stringstream.
    std::ostream& logStream();
    void setLogStream(std::ostream& os);
}
```

Then replace all `std::cout << ...` with `logStream() << ...` in:
- `src/search/pc.cpp` (6 occurrences)
- `src/search/fas.cpp` (1 occurrence)
- `src/search/meek_rules.cpp` (5 occurrences)
- `src/search/fges.cpp` (check)
- `src/search/fci_orient.cpp` (check)
- `src/search/gfci.cpp` (check)

The R bindings call `setLogStream(Rcpp::Rcout)` at package load time.

### 2. No Other Blockers

The C++ core is clean C++17 with only Eigen as a dependency. No filesystem access, no threading APIs, no platform-specific code. RcppEigen bundles Eigen, so no vendoring of Eigen needed.

## Package Structure

```
r/rtetrad/
├── DESCRIPTION
├── NAMESPACE
├── LICENSE
├── R/
│   ├── rtetrad-package.R          # Package-level docs, .onLoad
│   ├── pc.R                       # run_pc()
│   ├── fges.R                     # run_fges()
│   ├── gfci.R                     # run_gfci()
│   └── knowledge.R                # tetrad_knowledge(), add_forbidden(), etc.
├── src/
│   ├── rcpp_bindings.cpp          # Rcpp wrappers (~ mirrors tetrad_bindings.cpp)
│   ├── rcpp_init.cpp              # R_init_rtetrad, register routines
│   ├── Makevars                   # Dev build: relative paths to ../../src/
│   ├── Makevars.win               # Dev build: Windows variant
│   ├── Makevars.cran              # CRAN build: local tetrad/ directory
│   └── Makevars.cran.win          # CRAN build: Windows variant
├── man/                           # roxygen2-generated
├── tests/
│   ├── testthat.R
│   └── testthat/
│       ├── test-pc.R
│       ├── test-fges.R
│       ├── test-gfci.R
│       └── test-knowledge.R
├── vignettes/
│   └── introduction.Rmd
└── .Rbuildignore
```

## Makevars (Development Build)

```makefile
CXX_STD = CXX17
TETRAD_SRC = ../../src

PKG_CXXFLAGS = $(shell "${R_HOME}/bin/Rscript" -e "RcppEigen:::CxxFlags()") \
               -I$(TETRAD_SRC)

PKG_LIBS = $(shell "${R_HOME}/bin/Rscript" -e "Rcpp:::LdFlags()") \
           $(LAPACK_LIBS) $(BLAS_LIBS) $(FLIBS)

TETRAD_OBJECTS = \
    $(TETRAD_SRC)/graph/node.o \
    $(TETRAD_SRC)/graph/edge.o \
    $(TETRAD_SRC)/graph/graph.o \
    $(TETRAD_SRC)/data/data_set.o \
    $(TETRAD_SRC)/data/knowledge.o \
    $(TETRAD_SRC)/search/ind_test_fisher_z.o \
    $(TETRAD_SRC)/search/sepset_map.o \
    $(TETRAD_SRC)/search/fas.o \
    $(TETRAD_SRC)/search/meek_rules.o \
    $(TETRAD_SRC)/search/pc.o \
    $(TETRAD_SRC)/search/sem_bic_score.o \
    $(TETRAD_SRC)/search/fges.o \
    $(TETRAD_SRC)/search/fci_orient.o \
    $(TETRAD_SRC)/search/gfci.o \
    $(TETRAD_SRC)/util/choice_generator.o

OBJECTS = rcpp_bindings.o $(TETRAD_OBJECTS)
```

## Makevars.cran (Vendored Build)

```makefile
CXX_STD = CXX17

PKG_CXXFLAGS = $(shell "${R_HOME}/bin/Rscript" -e "RcppEigen:::CxxFlags()") \
               -Itetrad

PKG_LIBS = $(shell "${R_HOME}/bin/Rscript" -e "Rcpp:::LdFlags()") \
           $(LAPACK_LIBS) $(BLAS_LIBS) $(FLIBS)

TETRAD_OBJECTS = \
    tetrad/graph/node.o \
    tetrad/graph/edge.o \
    tetrad/graph/graph.o \
    tetrad/data/data_set.o \
    tetrad/data/knowledge.o \
    tetrad/search/ind_test_fisher_z.o \
    tetrad/search/sepset_map.o \
    tetrad/search/fas.o \
    tetrad/search/meek_rules.o \
    tetrad/search/pc.o \
    tetrad/search/sem_bic_score.o \
    tetrad/search/fges.o \
    tetrad/search/fci_orient.o \
    tetrad/search/gfci.o \
    tetrad/util/choice_generator.o

OBJECTS = rcpp_bindings.o $(TETRAD_OBJECTS)
```

## R API Design

R-idiomatic: plain functions returning data frames and lists. No R5/R6 classes needed — the algorithms are stateless run-and-return operations.

### Core Functions

```r
# PC algorithm — returns list with $edges (data.frame), $graph (adjacency list)
run_pc(data, alpha = 0.05, depth = -1L, knowledge = NULL, verbose = FALSE)

# FGES — same return structure, adds $model_score
run_fges(data, penalty_discount = 1.0, faithfulness_assumed = TRUE,
         max_degree = -1L, knowledge = NULL, verbose = FALSE)

# GFCI — same return structure (PAG edges include circle endpoints)
run_gfci(data, alpha = 0.05, penalty_discount = 1.0, depth = -1L,
         max_degree = -1L, complete_rule_set = TRUE,
         max_disc_path_length = -1L, faithfulness_assumed = TRUE,
         knowledge = NULL, verbose = FALSE)
```

### Return Value

All three functions return an S3 object of class `"tetrad_result"`:

```r
result <- run_pc(data, alpha = 0.05)

result$edges
#>   from   to endpoint1 endpoint2 edge_type
#> 1    X    Y      TAIL     ARROW       -->
#> 2    Y    Z      TAIL      TAIL       ---

result$nodes        # character vector
result$num_edges    # integer
result$num_nodes    # integer
result$model_score  # numeric (FGES only, NA otherwise)

print(result)
# Tetrad PC result: 5 nodes, 4 edges
# X --> Y
# Y --- Z
# ...
```

The `$edges` data frame with parsed columns (from, to, endpoint1, endpoint2, edge_type) is more useful in R than raw strings since users can filter/join with dplyr.

### Knowledge API

```r
k <- tetrad_knowledge()
k <- add_forbidden(k, "X", "Y")
k <- add_required(k, "A", "B")
k <- set_tier(k, 0L, c("X_lag", "Y_lag"))
k <- set_tier(k, 1L, c("X", "Y"))

result <- run_pc(data, alpha = 0.05, knowledge = k)
```

Knowledge is an S3 wrapper around an Rcpp external pointer to the C++ `Knowledge` object. The `add_*`/`set_*` functions modify in place and return invisibly for pipe-chaining.

## Rcpp Bindings Layer

`rcpp_bindings.cpp` — thin layer mirroring `tetrad_bindings.cpp`:

```cpp
#include <RcppEigen.h>
#include "search/pc.h"
#include "search/fges.h"
#include "search/gfci.h"
#include "search/sem_bic_score.h"
#include "search/ind_test_fisher_z.h"
#include "data/data_set.h"
#include "data/knowledge.h"

using namespace tetrad;

// [[Rcpp::depends(RcppEigen)]]

// [[Rcpp::export]]
Rcpp::List run_pc_cpp(const Eigen::Map<Eigen::MatrixXd>& data,
                      Rcpp::CharacterVector col_names,
                      double alpha,
                      int depth,
                      bool verbose,
                      SEXP knowledge_ptr) {
    std::vector<std::string> names = Rcpp::as<std::vector<std::string>>(col_names);
    Eigen::MatrixXd data_copy = data;  // Map -> owned copy

    DataSet ds(data_copy, names);
    IndTestFisherZ test(ds, alpha);

    Pc pc(&test);
    pc.setDepth(depth);
    pc.setVerbose(verbose);

    if (!Rf_isNull(knowledge_ptr)) {
        Rcpp::XPtr<Knowledge> kptr(knowledge_ptr);
        pc.setKnowledge(*kptr);
    }

    Graph g = pc.search();

    // Build edge data frame
    auto edges = g.getEdges();
    std::sort(edges.begin(), edges.end());

    Rcpp::CharacterVector from_vec, to_vec, ep1_vec, ep2_vec, type_vec;
    for (const auto& e : edges) {
        from_vec.push_back(e.getNode1()->getName());
        to_vec.push_back(e.getNode2()->getName());
        // ... endpoint and type strings
    }

    Rcpp::DataFrame edge_df = Rcpp::DataFrame::create(
        Rcpp::Named("from") = from_vec,
        Rcpp::Named("to") = to_vec,
        Rcpp::Named("endpoint1") = ep1_vec,
        Rcpp::Named("endpoint2") = ep2_vec,
        Rcpp::Named("edge_type") = type_vec,
        Rcpp::Named("stringsAsFactors") = false
    );

    return Rcpp::List::create(
        Rcpp::Named("edges") = edge_df,
        Rcpp::Named("nodes") = Rcpp::wrap(g.getNodeNames()),
        Rcpp::Named("num_edges") = g.getNumEdges(),
        Rcpp::Named("num_nodes") = g.getNumNodes()
    );
}

// Similar for run_fges_cpp, run_gfci_cpp
// Knowledge constructor/mutator functions:
// [[Rcpp::export]]
SEXP create_knowledge() { return Rcpp::XPtr<Knowledge>(new Knowledge()); }
// [[Rcpp::export]]
void knowledge_set_forbidden(SEXP ptr, std::string from, std::string to) { ... }
// etc.
```

## Vendor Script

`scripts/vendor_for_cran.sh`:

```bash
#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(dirname "$SCRIPT_DIR")"
PKG="$ROOT/r/rtetrad"
DEST="$PKG/src/tetrad"

echo "Vendoring C++ source into $DEST"

rm -rf "$DEST"
mkdir -p "$DEST"

# Copy all C++ source (preserving directory structure)
for dir in graph data search util; do
    mkdir -p "$DEST/$dir"
    cp "$ROOT/src/$dir/"*.h "$DEST/$dir/"
    cp "$ROOT/src/$dir/"*.cpp "$DEST/$dir/"
done

# Swap to CRAN Makevars
cp "$PKG/src/Makevars.cran" "$PKG/src/Makevars"
if [ -f "$PKG/src/Makevars.cran.win" ]; then
    cp "$PKG/src/Makevars.cran.win" "$PKG/src/Makevars.win"
fi

echo "Done. Run 'R CMD build r/rtetrad' to create tarball."
```

## Implementation Phases

### Phase 0: C++ Prep (prerequisite) ✓ DONE
- [x] Add `src/util/log_stream.h` and `log_stream.cpp` with configurable ostream
- [x] Replace all `std::cout` in search code with `logStream()`
- [x] Verify C++ tests and Python bindings still work

### Phase 1: Scaffold + PC/FGES/GFCI ✓ DONE
- [x] Create `r/rtetrad/` directory structure
- [x] Write `DESCRIPTION` (Title, Imports: Rcpp, LinkingTo: Rcpp, RcppEigen)
- [x] Write `configure` script (creates symlinks for monorepo dev build)
- [x] Write `Makevars` and `Makevars.win` using local `tetrad/` paths (symlinked by configure)
- [x] Write `rcpp_bindings.cpp` with `run_pc_cpp()`, `run_fges_cpp()`, `run_gfci_cpp()` and Knowledge helpers
- [x] Write `R/pc.R`, `R/fges.R`, `R/gfci.R` with roxygen2 docs
- [x] Write `R/knowledge.R` with knowledge helpers
- [x] Write `R/rtetrad-package.R` with `.onLoad` (set log stream to Rcpp::Rcout)
- [x] Write `R/utils.R` with `prepare_data()` helper
- [x] Write `tests/testthat/test-pc.R`, `test-fges.R`, `test-gfci.R`, `test-knowledge.R`
- [x] S3 print method for `tetrad_result` (registered in NAMESPACE)
- [x] Package installs and all 33 tests pass

**Key design decision**: R's staged installation copies package to a temp dir, breaking relative paths to `../../src/`. Solved with a `configure` script that creates symlinks from `src/tetrad/{graph,data,search,util}` → canonical source. Makevars uses `-Itetrad` and local `tetrad/` paths.

### Phase 2: Polish
- [ ] Generate roxygen2 man pages (`devtools::document()`)
- [ ] Add `R CMD check` CI (GitHub Actions)
- [ ] Verify edge_type extraction for all edge types (directed, bidirected, partially oriented)
- [ ] Add more edge cases to tests

### Phase 3: CRAN Prep
- [ ] Write vendor script (`scripts/vendor_for_cran.sh`)
- [ ] Write `Makevars.cran` and `Makevars.cran.win`
- [ ] Verify vendored build works: `R CMD build` + `R CMD check --as-cran`
- [ ] Write vignette (`vignettes/introduction.Rmd`)
- [ ] Test on Windows (GitHub Actions or win-builder.r-project.org)
- [ ] Submit to CRAN

## Documentation

### roxygen2 Function Docs

Every exported function gets full roxygen2 documentation. Generated `.Rd` files go in `man/`.

#### run_pc.R

```r
#' Run the PC Algorithm for Causal Discovery
#'
#' PC is a constraint-based algorithm that uses conditional independence tests
#' (Fisher Z) to discover causal structure. It assumes causal sufficiency
#' (no latent confounders) and returns a CPDAG (Completed Partially Directed
#' Acyclic Graph).
#'
#' @param data A numeric matrix or data.frame. Rows are observations, columns
#'   are variables. All columns must be numeric. If a data.frame is provided,
#'   column names are used as variable names.
#' @param alpha Significance level for conditional independence tests.
#'   Lower values produce sparser graphs. Default: 0.05.
#' @param depth Maximum size of conditioning sets. -1 (default) means
#'   unlimited. Setting this to a small value (e.g., 3) speeds up the
#'   algorithm but may miss some conditional independencies.
#' @param knowledge Optional background knowledge created by
#'   \code{\link{tetrad_knowledge}}. Encodes forbidden/required edges and
#'   temporal tiers.
#' @param verbose If TRUE, print progress messages during search.
#'   Default: FALSE.
#'
#' @return A \code{tetrad_result} object (S3 list) with components:
#' \describe{
#'   \item{edges}{A data.frame with columns: \code{from}, \code{to},
#'     \code{endpoint1}, \code{endpoint2}, \code{edge_type}. Edge types are
#'     \code{"-->"} (directed) or \code{"---"} (undirected).}
#'   \item{nodes}{Character vector of variable names.}
#'   \item{num_edges}{Number of edges in the result graph.}
#'   \item{num_nodes}{Number of nodes.}
#'   \item{algorithm}{Character string: "PC".}
#'   \item{model_score}{NA (not applicable for constraint-based algorithms).}
#' }
#'
#' @examples
#' # Simple example with simulated data: X -> Y -> Z
#' set.seed(42)
#' n <- 500
#' X <- rnorm(n)
#' Y <- 0.8 * X + rnorm(n, sd = 0.5)
#' Z <- 0.6 * Y + rnorm(n, sd = 0.5)
#' data <- data.frame(X = X, Y = Y, Z = Z)
#'
#' result <- run_pc(data, alpha = 0.05)
#' print(result)
#' result$edges
#'
#' # With a stricter alpha (sparser graph)
#' result_strict <- run_pc(data, alpha = 0.01)
#'
#' @seealso \code{\link{run_fges}} for score-based search,
#'   \code{\link{run_gfci}} for latent variable models,
#'   \code{\link{tetrad_knowledge}} for background knowledge.
#'
#' @references
#' Spirtes, P., Glymour, C., & Scheines, R. (2000).
#' \emph{Causation, Prediction, and Search} (2nd ed.). MIT Press.
#'
#' @export
run_pc <- function(data, alpha = 0.05, depth = -1L, knowledge = NULL,
                   verbose = FALSE) {
  # ...
}
```

#### run_fges.R

```r
#' Run the FGES Algorithm for Causal Discovery
#'
#' FGES (Fast Greedy Equivalence Search) is a score-based algorithm that
#' searches over Markov equivalence classes using a greedy forward-backward
#' strategy with BIC scoring. Assumes causal sufficiency. Returns a CPDAG.
#' Generally faster than PC for large, sparse graphs.
#'
#' @param data A numeric matrix or data.frame.
#' @param penalty_discount BIC penalty multiplier. 1.0 = standard BIC.
#'   Higher values (e.g., 2.0) produce sparser graphs. Default: 1.0.
#' @param faithfulness_assumed If TRUE (default), skips the backward
#'   unfaithfulness phase, which is faster. Set to FALSE if you suspect
#'   near-unfaithful distributions.
#' @param max_degree Maximum node degree in the output graph. -1 (default)
#'   means unlimited.
#' @param knowledge Optional background knowledge. See
#'   \code{\link{tetrad_knowledge}}.
#' @param verbose If TRUE, print progress. Default: FALSE.
#'
#' @return A \code{tetrad_result} object. Same structure as \code{\link{run_pc}},
#'   but \code{$model_score} contains the BIC score of the final model (lower
#'   is better).
#'
#' @examples
#' set.seed(42)
#' n <- 1000
#' X1 <- rnorm(n)
#' X2 <- rnorm(n)
#' X3 <- 0.5 * X1 + 0.5 * X2 + rnorm(n, sd = 0.3)
#' X4 <- 0.7 * X3 + rnorm(n, sd = 0.3)
#' data <- data.frame(X1 = X1, X2 = X2, X3 = X3, X4 = X4)
#'
#' result <- run_fges(data)
#' print(result)
#' result$model_score
#'
#' # Sparser graph with higher penalty
#' result_sparse <- run_fges(data, penalty_discount = 2.0)
#'
#' @seealso \code{\link{run_pc}}, \code{\link{run_gfci}},
#'   \code{\link{tetrad_knowledge}}
#'
#' @references
#' Ramsey, J., Glymour, M., Sanchez-Romero, R., & Glymour, C. (2017).
#' A million variables and more: the Fast Greedy Equivalence Search algorithm
#' for learning high-dimensional graphical causal models, with an application
#' to functional magnetic resonance images.
#' \emph{International Journal of Data Science and Analytics}, 3, 121--129.
#'
#' @export
run_fges <- function(data, penalty_discount = 1.0, faithfulness_assumed = TRUE,
                     max_degree = -1L, knowledge = NULL, verbose = FALSE) {
  # ...
}
```

#### run_gfci.R

```r
#' Run the GFCI Algorithm for Causal Discovery with Latent Variables
#'
#' GFCI (Greedy FCI) is a hybrid algorithm combining score-based search (FGES)
#' with FCI orientation rules. Unlike PC and FGES, GFCI does \strong{not}
#' assume causal sufficiency — it can detect latent (unmeasured) confounders.
#' Returns a PAG (Partial Ancestral Graph).
#'
#' PAG edge types:
#' \describe{
#'   \item{\code{-->}}{Directed: definite causal relationship}
#'   \item{\code{---}}{Undirected}
#'   \item{\code{<->}}{Bidirected: latent common cause}
#'   \item{\code{o->}}{Partially oriented: circle endpoint}
#'   \item{\code{o-o}}{Fully ambiguous orientation}
#' }
#'
#' @param data A numeric matrix or data.frame.
#' @param alpha Significance level for independence tests. Default: 0.05.
#' @param penalty_discount BIC penalty for the FGES phase. Default: 1.0.
#' @param depth Maximum conditioning set size. Default: -1 (unlimited).
#' @param max_degree Maximum node degree. Default: -1 (unlimited).
#' @param complete_rule_set If TRUE (default), use Zhang's complete FCI
#'   rules R1-R10. If FALSE, use Spirtes' R1-R4 only.
#' @param max_disc_path_length Maximum discriminating path length for
#'   orientation rule R4. Default: -1 (unlimited).
#' @param faithfulness_assumed Faithfulness assumption for FGES phase.
#'   Default: TRUE.
#' @param knowledge Optional background knowledge. See
#'   \code{\link{tetrad_knowledge}}.
#' @param verbose If TRUE, print progress. Default: FALSE.
#'
#' @return A \code{tetrad_result} object. The \code{$edges} data frame may
#'   include PAG-specific edge types (bidirected, partially oriented, circle).
#'
#' @examples
#' # Simulate data with a latent confounder L:
#' # L -> X, L -> Y, X -> Z
#' set.seed(42)
#' n <- 500
#' L <- rnorm(n)               # latent (not in data)
#' X <- 0.8 * L + rnorm(n, sd = 0.5)
#' Y <- 0.8 * L + rnorm(n, sd = 0.5)
#' Z <- 0.6 * X + rnorm(n, sd = 0.5)
#' data <- data.frame(X = X, Y = Y, Z = Z)  # L excluded
#'
#' result <- run_gfci(data, alpha = 0.05)
#' print(result)
#' # Should show X <-> Y (bidirected, indicating latent common cause)
#'
#' @seealso \code{\link{run_pc}}, \code{\link{run_fges}},
#'   \code{\link{tetrad_knowledge}}
#'
#' @references
#' Ogarrio, J. M., Spirtes, P., & Ramsey, J. (2016).
#' A hybrid causal search algorithm for latent variable models.
#' \emph{Proceedings of the Eighth International Conference on
#' Probabilistic Graphical Models (PGM)}, 368--379.
#'
#' @export
run_gfci <- function(data, alpha = 0.05, penalty_discount = 1.0,
                     depth = -1L, max_degree = -1L,
                     complete_rule_set = TRUE,
                     max_disc_path_length = -1L,
                     faithfulness_assumed = TRUE,
                     knowledge = NULL, verbose = FALSE) {
  # ...
}
```

#### knowledge.R

```r
#' Create Background Knowledge for Causal Discovery
#'
#' Creates an empty knowledge object that can encode constraints for causal
#' search algorithms: forbidden edges, required edges, and temporal tiers.
#'
#' @return A \code{tetrad_knowledge} object (S3 wrapper around a C++ pointer).
#'
#' @examples
#' k <- tetrad_knowledge()
#'
#' # Forbid an edge direction
#' k <- add_forbidden(k, "Age", "BirthYear")
#'
#' # Require an edge direction
#' k <- add_required(k, "Treatment", "Outcome")
#'
#' # Set temporal tiers (earlier tiers cannot be caused by later tiers)
#' k <- set_tier(k, 0L, c("Age", "Gender"))         # background variables
#' k <- set_tier(k, 1L, c("Treatment"))              # intervention
#' k <- set_tier(k, 2L, c("Outcome", "SideEffect"))  # outcomes
#'
#' # Use with any algorithm
#' result <- run_pc(data, knowledge = k)
#'
#' @seealso \code{\link{add_forbidden}}, \code{\link{add_required}},
#'   \code{\link{set_tier}}, \code{\link{set_tier_forbidden_within}}
#'
#' @export
tetrad_knowledge <- function() {
  # ...
}

#' Add a Forbidden Edge to Knowledge
#'
#' Forbids the directed edge \code{from -> to}. The reverse direction
#' \code{to -> from} is still allowed unless separately forbidden.
#'
#' @param knowledge A \code{tetrad_knowledge} object.
#' @param from Character: source variable name.
#' @param to Character: target variable name.
#' @return The modified knowledge object (invisibly, for pipe-chaining).
#'
#' @examples
#' k <- tetrad_knowledge()
#' k <- add_forbidden(k, "Age", "BirthYear")
#' k <- add_forbidden(k, "Gender", "BirthYear")
#'
#' @export
add_forbidden <- function(knowledge, from, to) {
  # ...
}

#' Add a Required Edge to Knowledge
#'
#' Requires the directed edge \code{from -> to} to appear in the result.
#'
#' @param knowledge A \code{tetrad_knowledge} object.
#' @param from Character: source variable name.
#' @param to Character: target variable name.
#' @return The modified knowledge object (invisibly, for pipe-chaining).
#'
#' @export
add_required <- function(knowledge, from, to) {
  # ...
}

#' Set Variables in a Temporal Tier
#'
#' Variables in lower-numbered tiers are assumed to temporally precede
#' variables in higher-numbered tiers. Edges from higher tiers to lower
#' tiers are automatically forbidden.
#'
#' @param knowledge A \code{tetrad_knowledge} object.
#' @param tier Integer: tier number (0-based).
#' @param vars Character vector of variable names to place in this tier.
#' @return The modified knowledge object (invisibly, for pipe-chaining).
#'
#' @examples
#' k <- tetrad_knowledge()
#' k <- set_tier(k, 0L, c("X_lag1", "Y_lag1"))
#' k <- set_tier(k, 1L, c("X", "Y"))
#'
#' @export
set_tier <- function(knowledge, tier, vars) {
  # ...
}

#' Forbid Edges Within a Tier
#'
#' When set to TRUE, edges between variables within the same tier are
#' forbidden. Useful for contemporaneous variables that should not
#' directly cause each other.
#'
#' @param knowledge A \code{tetrad_knowledge} object.
#' @param tier Integer: tier number.
#' @param forbidden Logical: if TRUE, forbid within-tier edges.
#' @return The modified knowledge object (invisibly, for pipe-chaining).
#'
#' @export
set_tier_forbidden_within <- function(knowledge, tier, forbidden = TRUE) {
  # ...
}
```

### Package-Level Documentation

```r
#' rtetrad: Causal Discovery Algorithms from CMU's Tetrad
#'
#' R interface to high-performance C++ implementations of causal discovery
#' algorithms from the Tetrad project (Carnegie Mellon University). Supports
#' constraint-based (PC), score-based (FGES), and hybrid (GFCI) algorithms
#' with background knowledge constraints.
#'
#' @section Algorithms:
#' \describe{
#'   \item{\code{\link{run_pc}}}{Constraint-based. Assumes no latent
#'     confounders. Returns a CPDAG.}
#'   \item{\code{\link{run_fges}}}{Score-based (BIC). Assumes no latent
#'     confounders. Returns a CPDAG. Faster for large graphs.}
#'   \item{\code{\link{run_gfci}}}{Hybrid. Handles latent confounders.
#'     Returns a PAG.}
#' }
#'
#' @section Background Knowledge:
#' All algorithms accept optional background knowledge via
#' \code{\link{tetrad_knowledge}}. Knowledge can encode:
#' \itemize{
#'   \item Forbidden edges (e.g., "Age cannot cause BirthYear")
#'   \item Required edges (e.g., "Treatment must cause Outcome")
#'   \item Temporal tiers (e.g., lag variables precede current variables)
#' }
#'
#' @docType package
#' @name rtetrad-package
#' @useDynLib rtetrad, .registration = TRUE
#' @importFrom Rcpp sourceCpp
NULL
```

## Examples

### README Examples

The package README (`r/rtetrad/README.md`) includes quick-start examples:

#### Basic PC

```r
library(rtetrad)

# Simulate a simple DAG: X -> Y -> Z, X -> Z
set.seed(42)
n <- 500
X <- rnorm(n)
Y <- 0.8 * X + rnorm(n, sd = 0.5)
Z <- 0.3 * X + 0.6 * Y + rnorm(n, sd = 0.5)
data <- data.frame(X = X, Y = Y, Z = Z)

result <- run_pc(data, alpha = 0.05)
print(result)
#> Tetrad PC result: 3 nodes, 3 edges
#>   X --> Y
#>   X --> Z
#>   Y --> Z

result$edges
#>   from to endpoint1 endpoint2 edge_type
#> 1    X  Y      TAIL     ARROW       -->
#> 2    X  Z      TAIL     ARROW       -->
#> 3    Y  Z      TAIL     ARROW       -->
```

#### Comparing Algorithms

```r
library(rtetrad)

# Same data, three algorithms
data <- read.csv("mydata.csv")

pc_result   <- run_pc(data, alpha = 0.05)
fges_result <- run_fges(data, penalty_discount = 1.0)
gfci_result <- run_gfci(data, alpha = 0.05)

# Compare edge counts
cat("PC edges:",   pc_result$num_edges, "\n")
cat("FGES edges:", fges_result$num_edges, "(score:", fges_result$model_score, ")\n")
cat("GFCI edges:", gfci_result$num_edges, "\n")

# GFCI can detect latent confounders (bidirected edges)
bidirected <- gfci_result$edges[gfci_result$edges$edge_type == "<->", ]
if (nrow(bidirected) > 0) {
  cat("Possible latent confounders between:\n")
  print(bidirected[, c("from", "to")])
}
```

#### Time-Series with Knowledge

```r
library(rtetrad)

# Panel data: add lags and enforce temporal ordering
data <- read.csv("timeseries.csv")  # columns: GDP, Inflation, Unemployment

# Create lagged variables
data_lagged <- data
for (col in names(data)) {
  data_lagged[[paste0(col, "_lag1")]] <- c(NA, head(data[[col]], -1))
}
data_lagged <- na.omit(data_lagged)

# Temporal knowledge: lags precede current values
k <- tetrad_knowledge()
k <- set_tier(k, 0L, c("GDP_lag1", "Inflation_lag1", "Unemployment_lag1"))
k <- set_tier(k, 1L, c("GDP", "Inflation", "Unemployment"))
k <- set_tier_forbidden_within(k, 0L, TRUE)  # no edges among lags

result <- run_pc(data_lagged, alpha = 0.05, knowledge = k)
print(result)
```

#### Working with Results in tidyverse

```r
library(rtetrad)
library(dplyr)

result <- run_fges(data)

# Filter to directed edges only
result$edges %>%
  filter(edge_type == "-->") %>%
  select(from, to)

# Find all parents of a variable
parents_of_Y <- result$edges %>%
  filter(to == "Y", edge_type == "-->") %>%
  pull(from)

# Find all children of a variable
children_of_X <- result$edges %>%
  filter(from == "X", edge_type == "-->") %>%
  pull(to)
```

#### Knowledge Tutorial (R equivalent of `examples/python/knowledge_tutorial.ipynb`)

```r
# ===================================================================
# Background Knowledge in Causal Discovery — R Tutorial
# ===================================================================
#
# Causal discovery algorithms learn graph structure from data alone,
# but they often leave some edges undirected or miss orientations.
# Background knowledge lets you encode domain expertise to constrain
# the search and resolve ambiguities.
#
# rtetrad supports three types of knowledge:
#   1. Temporal tiers — earlier tiers cannot be caused by later tiers
#   2. Forbidden edges — explicitly forbid specific directed edges
#   3. Required edges — explicitly require specific directed edges
#
# This tutorial uses an 8-variable clinical dataset to show how each
# type of knowledge progressively improves the discovered graph.
# ===================================================================

library(rtetrad)

# ------------------------------------------------------------------
# The Clinical Dataset
# ------------------------------------------------------------------
# True causal structure:
#
#   Tier 0 (background):  Age, Genetics
#   Tier 1 (lifestyle):   Exercise <- Age;  Diet;  Smoking
#   Tier 2 (biomarkers):  BMI <- Age, Exercise, Diet
#                          Cholesterol <- Genetics, Diet, Smoking
#                          BP <- Genetics, BMI, Smoking
#   Tier 3 (outcome):     HeartRisk <- Cholesterol, BP

set.seed(42)
n <- 5000

# Tier 0: Background factors
Age      <- rnorm(n)
Genetics <- rnorm(n)

# Tier 1: Lifestyle factors
Exercise <- 0.4 * Age + 0.7 * rnorm(n)
Diet     <- rnorm(n)
Smoking  <- rnorm(n)

# Tier 2: Biomarkers
BMI         <- 0.3 * Age + 0.4 * Exercise + 0.3 * Diet + 0.5 * rnorm(n)
Cholesterol <- 0.5 * Genetics + 0.4 * Diet + 0.3 * Smoking + 0.5 * rnorm(n)
BP          <- 0.4 * Genetics + 0.3 * BMI + 0.4 * Smoking + 0.5 * rnorm(n)

# Tier 3: Outcome
HeartRisk <- 0.5 * Cholesterol + 0.5 * BP + 0.4 * rnorm(n)

df <- data.frame(
  Age = Age, Genetics = Genetics, Exercise = Exercise,
  Diet = Diet, Smoking = Smoking, BMI = BMI,
  Cholesterol = Cholesterol, BP = BP, HeartRisk = HeartRisk
)

cat(sprintf("Data: %d samples, %d variables\n", nrow(df), ncol(df)))

# ------------------------------------------------------------------
# 1. Baseline — No Knowledge
# ------------------------------------------------------------------
# The algorithm discovers the skeleton but may leave edges undirected
# because data alone cannot always distinguish causal direction.

result_none <- run_pc(df, alpha = 0.01)

cat(sprintf("\nBaseline: %d edges\n", result_none$num_edges))
directed   <- sum(result_none$edges$edge_type == "-->")
undirected <- sum(result_none$edges$edge_type == "---")
cat(sprintf("  directed: %d, undirected: %d\n", directed, undirected))

# Show edges, flagging undirected ones
for (i in seq_len(nrow(result_none$edges))) {
  e <- result_none$edges[i, ]
  flag <- if (e$edge_type == "---") "  <<<" else ""
  cat(sprintf("  %s %s %s%s\n", e$from, e$edge_type, e$to, flag))
}

# ------------------------------------------------------------------
# 2. Temporal Tiers
# ------------------------------------------------------------------
# Temporal ordering: background -> lifestyle -> biomarkers -> outcome.
# Tiers forbid edges from later tiers to earlier tiers (e.g., BMI
# cannot cause Age, HeartRisk cannot cause Exercise).

k_tiers <- tetrad_knowledge()
k_tiers <- set_tier(k_tiers, 0L, c("Age", "Genetics"))
k_tiers <- set_tier(k_tiers, 1L, c("Exercise", "Diet", "Smoking"))
k_tiers <- set_tier(k_tiers, 2L, c("BMI", "Cholesterol", "BP"))
k_tiers <- set_tier(k_tiers, 3L, c("HeartRisk"))

result_tiers <- run_pc(df, alpha = 0.01, knowledge = k_tiers)

cat(sprintf("\nWith tiers: %d edges\n", result_tiers$num_edges))
for (i in seq_len(nrow(result_tiers$edges))) {
  e <- result_tiers$edges[i, ]
  cat(sprintf("  %s %s %s\n", e$from, e$edge_type, e$to))
}

# What changed?
edges_none  <- paste(result_none$edges$from, result_none$edges$edge_type, result_none$edges$to)
edges_tiers <- paste(result_tiers$edges$from, result_tiers$edges$edge_type, result_tiers$edges$to)

only_baseline <- setdiff(edges_none, edges_tiers)
only_tiers    <- setdiff(edges_tiers, edges_none)

if (length(only_baseline) > 0 || length(only_tiers) > 0) {
  cat("\nEdges in baseline but NOT with tiers:\n")
  for (e in sort(only_baseline)) cat(sprintf("  - %s\n", e))
  cat("\nEdges with tiers but NOT in baseline:\n")
  for (e in sort(only_tiers)) cat(sprintf("  + %s\n", e))
}

# ------------------------------------------------------------------
# 3. Forbidden Edges
# ------------------------------------------------------------------
# Within the same tier, forbid implausible direct paths:
#   - Exercise -> Cholesterol (indirect via BMI, not direct)
#   - Smoking -> BMI (smoking doesn't directly cause weight changes)

k_forbid <- tetrad_knowledge()
k_forbid <- set_tier(k_forbid, 0L, c("Age", "Genetics"))
k_forbid <- set_tier(k_forbid, 1L, c("Exercise", "Diet", "Smoking"))
k_forbid <- set_tier(k_forbid, 2L, c("BMI", "Cholesterol", "BP"))
k_forbid <- set_tier(k_forbid, 3L, c("HeartRisk"))
k_forbid <- add_forbidden(k_forbid, "Exercise", "Cholesterol")
k_forbid <- add_forbidden(k_forbid, "Smoking", "BMI")

result_forbid <- run_pc(df, alpha = 0.01, knowledge = k_forbid)

cat(sprintf("\nWith tiers + forbidden: %d edges\n", result_forbid$num_edges))
for (i in seq_len(nrow(result_forbid$edges))) {
  e <- result_forbid$edges[i, ]
  cat(sprintf("  %s %s %s\n", e$from, e$edge_type, e$to))
}

# Verify forbidden edges are absent
has_ex_chol <- any(result_forbid$edges$from == "Exercise" &
                   result_forbid$edges$to == "Cholesterol" &
                   result_forbid$edges$edge_type == "-->")
has_sm_bmi  <- any(result_forbid$edges$from == "Smoking" &
                   result_forbid$edges$to == "BMI" &
                   result_forbid$edges$edge_type == "-->")
cat(sprintf("\nExercise -> Cholesterol present? %s\n", has_ex_chol))
cat(sprintf("Smoking -> BMI present? %s\n", has_sm_bmi))

# ------------------------------------------------------------------
# 4. Required Edges
# ------------------------------------------------------------------
# Force well-established causal links into the result:
#   - Smoking -> BP (smoking raises blood pressure)
#   - Diet -> Cholesterol (dietary fat affects cholesterol)

k_full <- tetrad_knowledge()
k_full <- set_tier(k_full, 0L, c("Age", "Genetics"))
k_full <- set_tier(k_full, 1L, c("Exercise", "Diet", "Smoking"))
k_full <- set_tier(k_full, 2L, c("BMI", "Cholesterol", "BP"))
k_full <- set_tier(k_full, 3L, c("HeartRisk"))
k_full <- add_forbidden(k_full, "Exercise", "Cholesterol")
k_full <- add_forbidden(k_full, "Smoking", "BMI")
k_full <- add_required(k_full, "Smoking", "BP")
k_full <- add_required(k_full, "Diet", "Cholesterol")

result_full <- run_pc(df, alpha = 0.01, knowledge = k_full)

cat(sprintf("\nFull knowledge: %d edges\n", result_full$num_edges))
for (i in seq_len(nrow(result_full$edges))) {
  e <- result_full$edges[i, ]
  cat(sprintf("  %s %s %s\n", e$from, e$edge_type, e$to))
}

# Verify required edges
has_sm_bp   <- any(result_full$edges$from == "Smoking" &
                   result_full$edges$to == "BP" &
                   result_full$edges$edge_type == "-->")
has_di_chol <- any(result_full$edges$from == "Diet" &
                   result_full$edges$to == "Cholesterol" &
                   result_full$edges$edge_type == "-->")
cat(sprintf("\nSmoking -> BP present? %s\n", has_sm_bp))
cat(sprintf("Diet -> Cholesterol present? %s\n", has_di_chol))

# ------------------------------------------------------------------
# 5. Summary — Progressive Refinement
# ------------------------------------------------------------------

configs <- list(
  list(name = "No knowledge",       r = result_none),
  list(name = "+ Temporal tiers",    r = result_tiers),
  list(name = "+ Forbidden edges",   r = result_forbid),
  list(name = "+ Required edges",    r = result_full)
)

cat("\n")
for (cfg in configs) {
  r <- cfg$r
  dir   <- sum(r$edges$edge_type == "-->")
  undir <- sum(r$edges$edge_type == "---")
  cat(sprintf("%-25s  edges=%2d  directed=%2d  undirected=%2d\n",
              cfg$name, r$num_edges, dir, undir))
}

# ------------------------------------------------------------------
# Compare final result against the true causal graph
# ------------------------------------------------------------------
true_edges <- data.frame(
  from = c("Age", "Age", "Exercise", "Diet", "Diet", "Genetics",
           "Genetics", "Smoking", "Smoking", "BMI", "Cholesterol", "BP"),
  to   = c("Exercise", "BMI", "BMI", "BMI", "Cholesterol", "Cholesterol",
           "BP", "Cholesterol", "BP", "BP", "HeartRisk", "HeartRisk")
)

directed <- result_full$edges[result_full$edges$edge_type == "-->",
                              c("from", "to")]

correct <- merge(true_edges, directed)
missed  <- true_edges[!paste(true_edges$from, true_edges$to) %in%
                       paste(directed$from, directed$to), ]
extra   <- directed[!paste(directed$from, directed$to) %in%
                     paste(true_edges$from, true_edges$to), ]

cat(sprintf("\nTrue edges: %d\n", nrow(true_edges)))
cat(sprintf("Correctly discovered: %d / %d\n", nrow(correct), nrow(true_edges)))

if (nrow(missed) > 0) {
  cat("\nMissed:\n")
  for (i in seq_len(nrow(missed)))
    cat(sprintf("  %s --> %s\n", missed$from[i], missed$to[i]))
}
if (nrow(extra) > 0) {
  cat("\nExtra (false positives):\n")
  for (i in seq_len(nrow(extra)))
    cat(sprintf("  %s --> %s\n", extra$from[i], extra$to[i]))
}
if (nrow(missed) == 0 && nrow(extra) == 0) {
  cat("Perfect recovery!\n")
}

# ------------------------------------------------------------------
# Bonus: Forbid Edges Within a Tier
# ------------------------------------------------------------------
# Variables in the same tier may be known to be independent.
# set_tier_forbidden_within() forbids all directed edges between
# variables in the same tier.

k_within <- tetrad_knowledge()
k_within <- set_tier(k_within, 0L, c("Age", "Genetics"))
k_within <- set_tier(k_within, 1L, c("Exercise", "Diet", "Smoking"))
k_within <- set_tier(k_within, 2L, c("BMI", "Cholesterol", "BP"))
k_within <- set_tier(k_within, 3L, c("HeartRisk"))
k_within <- set_tier_forbidden_within(k_within, 0L)  # no Age <-> Genetics
k_within <- set_tier_forbidden_within(k_within, 1L)  # no Exercise/Diet/Smoking edges

result_within <- run_pc(df, alpha = 0.01, knowledge = k_within)

cat(sprintf("\nWith within-tier forbidden: %d edges\n", result_within$num_edges))
for (i in seq_len(nrow(result_within$edges))) {
  e <- result_within$edges[i, ]
  cat(sprintf("  %s %s %s\n", e$from, e$edge_type, e$to))
}
cat("\nNote: No edges between Age-Genetics or Exercise-Diet-Smoking\n")
```

### Vignette Outline

`vignettes/introduction.Rmd` — "Getting Started with rtetrad"

1. **Installation** — `install.packages("rtetrad")` or devtools
2. **Quick start** — Run PC on iris data (dropping Species), interpret edges
3. **Choosing an algorithm**
   - PC: when you trust causal sufficiency, moderate sample size
   - FGES: when you trust causal sufficiency, large/sparse graphs
   - GFCI: when latent confounders may exist
4. **Understanding the output**
   - CPDAG vs PAG
   - Edge types and what they mean
   - The `$edges` data frame columns
5. **Background knowledge**
   - Forbidden/required edges
   - Temporal tiers for time-series data
   - Example: economics panel data
6. **Tuning parameters**
   - `alpha`: significance level (PC, GFCI)
   - `penalty_discount`: BIC penalty (FGES, GFCI)
   - `depth`: conditioning set limit
   - `complete_rule_set`: Zhang's rules vs Spirtes'
7. **Comparison with other R packages**
   - pcalg: same algorithms but pure R/slower; rtetrad uses C++
   - bnlearn: focuses on Bayesian networks; different algorithm set

## CRAN Compliance Checklist

- [ ] No `std::cout` / `std::cerr` — all output via `Rcpp::Rcout` / `REprintf`
- [ ] No `exit()` or `abort()` — use `Rcpp::stop()` for errors
- [ ] No writing to filesystem (outside `tempdir()`)
- [ ] `R CMD check --as-cran` returns 0 errors, 0 warnings, 0 notes
- [ ] Builds on Linux, macOS, Windows
- [ ] All vendored source included (no downloads at install time)
- [ ] `DESCRIPTION` has valid `Authors@R`, `License`, `URL`, `BugReports`
- [ ] Examples run in < 5 seconds each
- [ ] Package title in Title Case, no period at end
- [ ] Description field > 1 sentence

## Dependencies Summary

| Component | Source |
|-----------|--------|
| Eigen | Bundled by RcppEigen (no separate vendoring) |
| Rcpp | CRAN |
| RcppEigen | CRAN |
| tetrad C++ core | Vendored from this repo at release time |
| Catch2 | Not needed (R tests use testthat) |
| nanobind | Not needed (Python only) |
