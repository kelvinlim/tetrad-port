#' Run the FGES Algorithm for Causal Discovery
#'
#' FGES (Fast Greedy Equivalence Search) is a score-based algorithm that
#' searches over Markov equivalence classes using a greedy forward-backward
#' strategy with BIC scoring. Assumes causal sufficiency. Returns a CPDAG.
#'
#' @param data A numeric matrix or data.frame.
#' @param penalty_discount BIC penalty multiplier. 1.0 = standard BIC.
#'   Higher values produce sparser graphs. Default: 1.0.
#' @param faithfulness_assumed If TRUE (default), skips the backward
#'   unfaithfulness phase.
#' @param max_degree Maximum node degree. -1 (default) means unlimited.
#' @param knowledge Optional background knowledge. See
#'   \code{\link{tetrad_knowledge}}.
#' @param verbose If TRUE, print progress. Default: FALSE.
#'
#' @return A \code{tetrad_result} object. Same structure as \code{\link{run_pc}},
#'   but \code{$model_score} contains the BIC score of the final model.
#'
#' @examples
#' set.seed(42)
#' n <- 1000
#' X1 <- rnorm(n)
#' X2 <- rnorm(n)
#' X3 <- 0.5 * X1 + 0.5 * X2 + rnorm(n, sd = 0.3)
#' data <- data.frame(X1 = X1, X2 = X2, X3 = X3)
#'
#' result <- run_fges(data)
#' print(result)
#'
#' @seealso \code{\link{run_pc}}, \code{\link{run_gfci}},
#'   \code{\link{tetrad_knowledge}}
#' @references
#' Ramsey, J., Glymour, M., Sanchez-Romero, R., & Glymour, C. (2017).
#' A million variables and more: the Fast Greedy Equivalence Search algorithm
#' for learning high-dimensional graphical causal models.
#' \emph{International Journal of Data Science and Analytics}, 3, 121--129.
#' @export
run_fges <- function(data, penalty_discount = 1.0, faithfulness_assumed = TRUE,
                     max_degree = -1L, knowledge = NULL, verbose = FALSE) {
  prepared <- prepare_data(data)
  kptr <- if (!is.null(knowledge)) knowledge$.ptr else NULL
  run_fges_cpp(prepared$matrix, prepared$col_names, penalty_discount,
               faithfulness_assumed, as.integer(max_degree), verbose, kptr)
}
