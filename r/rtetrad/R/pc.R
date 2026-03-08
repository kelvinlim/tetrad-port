#' Run the PC Algorithm for Causal Discovery
#'
#' PC is a constraint-based algorithm that uses conditional independence tests
#' (Fisher Z) to discover causal structure. It assumes causal sufficiency
#' (no latent confounders) and returns a CPDAG (Completed Partially Directed
#' Acyclic Graph).
#'
#' @param data A numeric matrix or data.frame. Rows are observations, columns
#'   are variables. All columns must be numeric.
#' @param alpha Significance level for conditional independence tests.
#'   Lower values produce sparser graphs. Default: 0.05.
#' @param depth Maximum size of conditioning sets. -1 (default) means
#'   unlimited.
#' @param knowledge Optional background knowledge created by
#'   \code{\link{tetrad_knowledge}}.
#' @param verbose If TRUE, print progress messages. Default: FALSE.
#'
#' @return A \code{tetrad_result} object (S3 list) with components:
#' \describe{
#'   \item{edges}{A data.frame with columns: \code{from}, \code{to},
#'     \code{endpoint1}, \code{endpoint2}, \code{edge_type}.}
#'   \item{nodes}{Character vector of variable names.}
#'   \item{num_edges}{Number of edges.}
#'   \item{num_nodes}{Number of nodes.}
#'   \item{algorithm}{Character: "PC".}
#'   \item{model_score}{NA (not applicable for PC).}
#' }
#'
#' @examples
#' set.seed(42)
#' n <- 500
#' X <- rnorm(n)
#' Y <- 0.8 * X + rnorm(n, sd = 0.5)
#' Z <- 0.6 * Y + rnorm(n, sd = 0.5)
#' data <- data.frame(X = X, Y = Y, Z = Z)
#'
#' result <- run_pc(data, alpha = 0.05)
#' print(result)
#'
#' @seealso \code{\link{run_fges}}, \code{\link{run_gfci}},
#'   \code{\link{tetrad_knowledge}}
#' @references
#' Spirtes, P., Glymour, C., & Scheines, R. (2000).
#' \emph{Causation, Prediction, and Search} (2nd ed.). MIT Press.
#' @export
run_pc <- function(data, alpha = 0.05, depth = -1L, knowledge = NULL,
                   verbose = FALSE) {
  prepared <- prepare_data(data)
  kptr <- if (!is.null(knowledge)) knowledge$.ptr else NULL
  run_pc_cpp(prepared$matrix, prepared$col_names, alpha,
             as.integer(depth), verbose, kptr)
}
