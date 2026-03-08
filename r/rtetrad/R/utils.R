# Internal helper to validate and extract data for C++ calls.
prepare_data <- function(data) {
  if (is.data.frame(data)) {
    col_names <- colnames(data)
    mat <- as.matrix(data)
    if (!is.numeric(mat)) {
      stop("All columns in data must be numeric.", call. = FALSE)
    }
    storage.mode(mat) <- "double"
  } else if (is.matrix(data)) {
    if (!is.numeric(data)) {
      stop("Data matrix must be numeric.", call. = FALSE)
    }
    mat <- data
    storage.mode(mat) <- "double"
    col_names <- colnames(data)
    if (is.null(col_names)) {
      col_names <- paste0("V", seq_len(ncol(data)))
    }
  } else {
    stop("data must be a data.frame or numeric matrix.", call. = FALSE)
  }

  list(matrix = mat, col_names = col_names)
}
