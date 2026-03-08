#' Create Background Knowledge for Causal Discovery
#'
#' Creates an empty knowledge object that can encode constraints for causal
#' search algorithms: forbidden edges, required edges, and temporal tiers.
#'
#' @return A \code{tetrad_knowledge} object.
#'
#' @examples
#' k <- tetrad_knowledge()
#' k <- add_forbidden(k, "Age", "BirthYear")
#' k <- set_tier(k, 0L, c("Age", "Gender"))
#' k <- set_tier(k, 1L, c("Outcome"))
#'
#' @seealso \code{\link{add_forbidden}}, \code{\link{add_required}},
#'   \code{\link{set_tier}}, \code{\link{set_tier_forbidden_within}}
#' @export
tetrad_knowledge <- function() {
  obj <- list(.ptr = create_knowledge_cpp())
  class(obj) <- "tetrad_knowledge"
  obj
}

#' Add a Forbidden Edge to Knowledge
#'
#' Forbids the directed edge \code{from -> to}. The reverse direction is
#' still allowed unless separately forbidden.
#'
#' @param knowledge A \code{tetrad_knowledge} object.
#' @param from Character: source variable name.
#' @param to Character: target variable name.
#' @return The modified knowledge object (invisibly, for pipe-chaining).
#'
#' @examples
#' k <- tetrad_knowledge()
#' k <- add_forbidden(k, "Age", "BirthYear")
#'
#' @export
add_forbidden <- function(knowledge, from, to) {
  knowledge_set_forbidden_cpp(knowledge$.ptr, from, to)
  invisible(knowledge)
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
  knowledge_set_required_cpp(knowledge$.ptr, from, to)
  invisible(knowledge)
}

#' Set Variables in a Temporal Tier
#'
#' Variables in lower-numbered tiers are assumed to temporally precede
#' variables in higher-numbered tiers. Edges from higher tiers to lower
#' tiers are automatically forbidden.
#'
#' @param knowledge A \code{tetrad_knowledge} object.
#' @param tier Integer: tier number (0-based).
#' @param vars Character vector of variable names.
#' @return The modified knowledge object (invisibly, for pipe-chaining).
#'
#' @examples
#' k <- tetrad_knowledge()
#' k <- set_tier(k, 0L, c("X_lag1", "Y_lag1"))
#' k <- set_tier(k, 1L, c("X", "Y"))
#'
#' @export
set_tier <- function(knowledge, tier, vars) {
  knowledge_set_tier_cpp(knowledge$.ptr, as.integer(tier), vars)
  invisible(knowledge)
}

#' Forbid Edges Within a Tier
#'
#' When set to TRUE, edges between variables within the same tier are
#' forbidden.
#'
#' @param knowledge A \code{tetrad_knowledge} object.
#' @param tier Integer: tier number.
#' @param forbidden Logical: if TRUE, forbid within-tier edges. Default: TRUE.
#' @return The modified knowledge object (invisibly, for pipe-chaining).
#'
#' @export
set_tier_forbidden_within <- function(knowledge, tier, forbidden = TRUE) {
  knowledge_set_tier_forbidden_within_cpp(knowledge$.ptr, as.integer(tier),
                                          forbidden)
  invisible(knowledge)
}
