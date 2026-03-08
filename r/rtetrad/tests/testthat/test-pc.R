test_that("run_pc discovers a simple chain X -> Y -> Z", {
  set.seed(42)
  n <- 1000
  X <- rnorm(n)
  Y <- 0.8 * X + rnorm(n, sd = 0.5)
  Z <- 0.6 * Y + rnorm(n, sd = 0.5)
  data <- data.frame(X = X, Y = Y, Z = Z)

  result <- run_pc(data, alpha = 0.05)

  expect_s3_class(result, "tetrad_result")
  expect_equal(result$algorithm, "PC")
  expect_equal(result$num_nodes, 3)
  expect_true(result$num_edges >= 2)
  expect_true(is.data.frame(result$edges))
  expect_true(all(c("from", "to", "endpoint1", "endpoint2", "edge_type")
                  %in% colnames(result$edges)))
  expect_true(is.na(result$model_score))
})

test_that("run_pc accepts matrix input", {
  set.seed(42)
  mat <- matrix(rnorm(300), ncol = 3)
  colnames(mat) <- c("A", "B", "C")

  result <- run_pc(mat, alpha = 0.05)
  expect_s3_class(result, "tetrad_result")
  expect_equal(result$num_nodes, 3)
})

test_that("run_pc works with knowledge", {
  set.seed(42)
  n <- 1000
  X <- rnorm(n)
  Y <- 0.8 * X + rnorm(n, sd = 0.5)
  data <- data.frame(X = X, Y = Y)

  k <- tetrad_knowledge()
  k <- add_forbidden(k, "X", "Y")

  result <- run_pc(data, alpha = 0.05, knowledge = k)
  expect_s3_class(result, "tetrad_result")

  # X -> Y should be forbidden; if there's an edge it should be Y -> X
  directed_xy <- result$edges$from == "X" & result$edges$to == "Y" &
                 result$edges$edge_type == "-->"
  expect_false(any(directed_xy))
})

test_that("run_pc print method works", {
  set.seed(42)
  n <- 500
  X <- rnorm(n)
  Y <- 0.8 * X + rnorm(n, sd = 0.5)
  data <- data.frame(X = X, Y = Y)

  result <- run_pc(data, alpha = 0.05)
  expect_output(print(result), "Tetrad PC result")
})

test_that("run_pc detects collider and produces directed edges", {
  set.seed(42)
  n <- 2000
  X <- rnorm(n)
  Z <- rnorm(n)
  Y <- 0.8 * X + 0.8 * Z + rnorm(n, sd = 0.3)
  data <- data.frame(X = X, Y = Y, Z = Z)

  result <- run_pc(data, alpha = 0.01)
  expect_equal(result$num_edges, 2)
  expect_true(all(result$edges$edge_type == "-->"))
  expect_true(all(result$edges$endpoint2 == "ARROW"))
})

test_that("run_pc generates V-prefixed names for unnamed matrix", {
  set.seed(42)
  mat <- matrix(rnorm(300), ncol = 3)

  result <- run_pc(mat, alpha = 0.05)
  expect_equal(sort(result$nodes), c("V1", "V2", "V3"))
})

test_that("run_pc rejects non-numeric data", {
  data <- data.frame(A = c("x", "y", "z"), B = 1:3)
  expect_error(run_pc(data), "numeric")
})

test_that("run_pc rejects non-data.frame/matrix input", {
  expect_error(run_pc(list(a = 1:3)), "data.frame or numeric matrix")
})
