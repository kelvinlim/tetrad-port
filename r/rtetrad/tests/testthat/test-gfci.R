test_that("run_gfci runs and returns PAG structure", {
  set.seed(42)
  n <- 500
  L <- rnorm(n)
  X <- 0.8 * L + rnorm(n, sd = 0.5)
  Y <- 0.8 * L + rnorm(n, sd = 0.5)
  Z <- 0.6 * X + rnorm(n, sd = 0.5)
  data <- data.frame(X = X, Y = Y, Z = Z)

  result <- run_gfci(data, alpha = 0.05)

  expect_s3_class(result, "tetrad_result")
  expect_equal(result$algorithm, "GFCI")
  expect_equal(result$num_nodes, 3)
  expect_true(result$num_edges >= 1)
  expect_true(is.data.frame(result$edges))
})

test_that("run_gfci works with knowledge", {
  set.seed(42)
  n <- 500
  X <- rnorm(n)
  Y <- 0.8 * X + rnorm(n, sd = 0.5)
  Z <- 0.6 * Y + rnorm(n, sd = 0.5)
  data <- data.frame(X = X, Y = Y, Z = Z)

  k <- tetrad_knowledge()
  k <- set_tier(k, 0L, c("X"))
  k <- set_tier(k, 1L, c("Y", "Z"))

  result <- run_gfci(data, alpha = 0.05, knowledge = k)
  expect_s3_class(result, "tetrad_result")
})
