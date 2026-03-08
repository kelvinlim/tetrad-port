test_that("run_fges discovers edges and returns model score", {
  set.seed(42)
  n <- 1000
  X1 <- rnorm(n)
  X2 <- rnorm(n)
  X3 <- 0.5 * X1 + 0.5 * X2 + rnorm(n, sd = 0.3)
  data <- data.frame(X1 = X1, X2 = X2, X3 = X3)

  result <- run_fges(data)

  expect_s3_class(result, "tetrad_result")
  expect_equal(result$algorithm, "FGES")
  expect_equal(result$num_nodes, 3)
  expect_true(result$num_edges >= 2)
  expect_false(is.na(result$model_score))
  expect_true(is.numeric(result$model_score))
})

test_that("run_fges penalty_discount affects sparsity", {
  set.seed(42)
  n <- 500
  X <- rnorm(n)
  Y <- 0.3 * X + rnorm(n, sd = 0.8)
  Z <- 0.3 * Y + rnorm(n, sd = 0.8)
  data <- data.frame(X = X, Y = Y, Z = Z)

  result_low  <- run_fges(data, penalty_discount = 0.5)
  result_high <- run_fges(data, penalty_discount = 4.0)

  # Higher penalty should produce same or fewer edges
  expect_true(result_high$num_edges <= result_low$num_edges)
})
