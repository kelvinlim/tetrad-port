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
#' @docType package
#' @name rtetrad-package
#' @useDynLib rtetrad, .registration = TRUE
#' @importFrom Rcpp sourceCpp
NULL

.onLoad <- function(libname, pkgname) {
  set_log_stream_rcout_cpp()
}

#' Print a tetrad_result object
#'
#' @param x A \code{tetrad_result} object.
#' @param ... Additional arguments (ignored).
#' @export
print.tetrad_result <- function(x, ...) {
  cat(sprintf("Tetrad %s result: %d nodes, %d edges\n",
              x$algorithm, x$num_nodes, x$num_edges))
  if (!is.na(x$model_score)) {
    cat(sprintf("Model score: %.4f\n", x$model_score))
  }
  if (x$num_edges > 0) {
    for (i in seq_len(nrow(x$edges))) {
      cat(sprintf("  %s %s %s\n",
                  x$edges$from[i], x$edges$edge_type[i], x$edges$to[i]))
    }
  }
  invisible(x)
}
