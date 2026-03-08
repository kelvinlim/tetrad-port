test_that("tetrad_knowledge creates a valid object", {
  k <- tetrad_knowledge()
  expect_s3_class(k, "tetrad_knowledge")
  expect_false(is.null(k$.ptr))
})

test_that("knowledge functions return invisibly for chaining", {
  k <- tetrad_knowledge()
  expect_invisible(add_forbidden(k, "A", "B"))
  expect_invisible(add_required(k, "C", "D"))
  expect_invisible(set_tier(k, 0L, c("A", "B")))
  expect_invisible(set_tier_forbidden_within(k, 0L))
})

test_that("knowledge with tiers affects PC results", {
  set.seed(42)
  n <- 1000
  X <- rnorm(n)
  Y <- 0.8 * X + rnorm(n, sd = 0.5)
  data <- data.frame(X = X, Y = Y)

  # Without knowledge
  r1 <- run_pc(data, alpha = 0.01)

  # With knowledge: Y cannot cause X
  k <- tetrad_knowledge()
  k <- set_tier(k, 0L, c("X"))
  k <- set_tier(k, 1L, c("Y"))
  r2 <- run_pc(data, alpha = 0.01, knowledge = k)

  expect_s3_class(r2, "tetrad_result")

  # With tiers, X -> Y should be directed (not undirected)
  if (r2$num_edges > 0) {
    directed_xy <- any(r2$edges$from == "X" & r2$edges$to == "Y" &
                       r2$edges$edge_type == "-->")
    undirected <- any(r2$edges$edge_type == "---")
    # Either X -> Y is directed, or no undirected edges remain
    expect_true(directed_xy || !undirected)
  }
})
