#' Run the GFCI Algorithm for Causal Discovery with Latent Variables
#'
#' GFCI (Greedy FCI) is a hybrid algorithm combining score-based search (FGES)
#' with FCI orientation rules. Unlike PC and FGES, GFCI does \strong{not}
#' assume causal sufficiency. Returns a PAG (Partial Ancestral Graph).
#'
#' @param data A numeric matrix or data.frame.
#' @param alpha Significance level for independence tests. Default: 0.05.
#' @param penalty_discount BIC penalty for the FGES phase. Default: 1.0.
#' @param depth Maximum conditioning set size. Default: -1 (unlimited).
#' @param max_degree Maximum node degree. Default: -1 (unlimited).
#' @param complete_rule_set If TRUE (default), use Zhang's complete FCI
#'   rules R1-R10. If FALSE, use Spirtes' R1-R4 only.
#' @param max_disc_path_length Maximum discriminating path length for
#'   rule R4. Default: -1 (unlimited).
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
#' set.seed(42)
#' n <- 500
#' L <- rnorm(n)
#' X <- 0.8 * L + rnorm(n, sd = 0.5)
#' Y <- 0.8 * L + rnorm(n, sd = 0.5)
#' Z <- 0.6 * X + rnorm(n, sd = 0.5)
#' data <- data.frame(X = X, Y = Y, Z = Z)
#'
#' result <- run_gfci(data, alpha = 0.05)
#' print(result)
#'
#' @seealso \code{\link{run_pc}}, \code{\link{run_fges}},
#'   \code{\link{tetrad_knowledge}}
#' @references
#' Ogarrio, J. M., Spirtes, P., & Ramsey, J. (2016).
#' A hybrid causal search algorithm for latent variable models.
#' \emph{Proceedings of PGM}, 368--379.
#' @export
run_gfci <- function(data, alpha = 0.05, penalty_discount = 1.0,
                     depth = -1L, max_degree = -1L,
                     complete_rule_set = TRUE,
                     max_disc_path_length = -1L,
                     faithfulness_assumed = TRUE,
                     knowledge = NULL, verbose = FALSE) {
  prepared <- prepare_data(data)
  kptr <- if (!is.null(knowledge)) knowledge$.ptr else NULL
  run_gfci_cpp(prepared$matrix, prepared$col_names, alpha, penalty_discount,
               as.integer(depth), as.integer(max_degree), complete_rule_set,
               as.integer(max_disc_path_length), faithfulness_assumed,
               verbose, kptr)
}
