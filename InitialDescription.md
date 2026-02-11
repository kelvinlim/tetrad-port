The tetrad repository is in the folder tetrad.

This is a massive undertaking. The tetrad-lib Java codebase is large, mature, and contains complex logic for graph theory and statistical independence testing. A "Big Bang" rewrite (porting everything at once) will likely fail.

The following plan focuses on a **"Vertical Slice" approach**: getting *one* core algorithm (e.g., PC) working end-to-end with Python/R bindings and robust testing before expanding.

### **Phase 1: Architecture & Tooling**

You need a modern C++ foundation that mirrors the Java logic where helpful, but uses C++ idioms for performance.

* **Language Standard:** **C++17** or **C++20** (for better standard library support).  
* **Build System:** **CMake**. It is the *only* viable choice for a project that needs to compile for C++, Python, and R simultaneously.  
* **Linear Algebra:** **Eigen**. Do not write your own matrix math. Eigen is header-only and standard in the C++ scientific community.  
* **Graph Library:**  
  * *Option A (Easier Port):* Re-implement a lightweight Node/Edge/Graph class structure that mirrors the Java edu.cmu.tetrad.graph package.  
  * *Option B (Better C++):* Use Boost.Graph. **Warning:** This will make the porting logic much harder because the APIs won't match. I recommend **Option A** for the initial port to ensure correctness.

### **Phase 2: The "Vertical Slice" (Proof of Concept)**

Do not try to port the whole library. Target the **PC Algorithm** with the **Fisher Z Test**.

1. **Core Types:** Port Node, Edge, Endpoint, and Graph.  
2. **Statistical Interface:** Port the IndependenceTest interface.  
3. **Implementation:** Port FisherZ (statistical test) and PcSearch (the search algorithm).  
4. **Result:** A function Graph run\_pc(Matrix data, double alpha).

### **Phase 3: The Bindings (Python & R)**

The key here is **Shared Memory**. You want to pass a matrix from R/Python to C++ without copying it.

#### **Python Strategy: nanobind**

* **Why:** It is the modern successor to pybind11. It compiles 2-3x faster and produces smaller binaries.  
* **Data Passing:** Use numpy arrays. nanobind can view a numpy array as an Eigen::Matrix wrapper automatically (zero-copy).  
* **Output:** Return the Graph as an Adjacency Matrix (numpy array) or a custom Python object.

#### **R Strategy: cpp11 or Rcpp**

* **Why:** Rcpp is the standard, but cpp11 is a newer, lighter-weight alternative that protects you from some of R's C API complexity.  
* **Data Passing:** R matrices map directly to C++ vectors/Eigen matrices.

### **Phase 4: Robust Testing (The "Golden Master" Strategy)**

This is the most critical part of your request. Since you are porting scientific code, you cannot rely on simple unit tests. You need **Conformance Testing**.

**The "Golden Master" Pipeline:**

1. **Reference Generator (Python Script):**  
   * Use py-tetrad (the existing Java wrapper) to run the *original* Java PC algorithm on a synthetic dataset.  
   * Serialize the output graph (edges, orientation) to a JSON file.  
2. **C++ Test Harness:**  
   * Load the same dataset.  
   * Run your new C++ implementation.  
   * Compare the result against the JSON "Golden Master".  
3. **Unit Tests (Catch2):**  
   * Test individual components (e.g., "Does Fisher Z return the correct p-value for this correlation?").

### **Proposed Repository Structure**

Plaintext

tetrad-cpp/  
├── CMakeLists.txt          \# Master build script  
├── src/                    \# Core C++ logic (No Python/R code here)  
│   ├── graph/  
│   ├── search/  
│   └── test/               \# Independence tests (FisherZ, etc.)  
├── bindings/  
│   ├── python/             \# nanobind definitions  
│   └── r/                  \# Rcpp/cpp11 definitions  
├── tests/  
│   ├── cpp/                \# Catch2 unit tests  
│   ├── python/             \# Pytest integration tests  
│   └── conformance/        \# The "Golden Master" data and scripts  
└── external/               \# Eigen, Catch2, json library

### **Step-by-Step Execution Plan**

1. **Week 1-2:** Set up CMake to build a simple "Hello World" shared library that can be imported in Python and R.  
2. **Week 3-6:** Port FisherZ and the basic Graph data structure. Verify FisherZ outputs match Java exactly (down to floating point precision).  
3. **Week 7-10:** Port the PC algorithm skeleton (adjacency search).  
4. **Week 11-12:** Port the PC orientation rules (collider, rule 1-3).  
5. **Ongoing:** Every time you implement a function, run the Conformance Test against the Java version.

**Would you like me to generate a CMakeLists.txt template that handles this three-language build configuration?**