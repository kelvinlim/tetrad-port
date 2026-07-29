# References

The primary literature behind every algorithm in this port, so the code can be
read against the papers rather than against Java Tetrad alone.

`papers/` holds the PDFs. Everything there is legally free — JMLR, PMLR,
NeurIPS proceedings, arXiv, Project Euclid, JSS, or a CMU/figshare repository —
and is committed so the sources are available offline and pinned to the version
this port was checked against.

`FunctionMapping.md` maps individual C++ functions to the exact theorem, lemma,
definition and rule numbers in these papers, and records every place the code
knowingly departs from them.

## What is not here

Three works are load-bearing but are not open access, so they are cited rather
than bundled:

| Work | Why not | How to get it |
|---|---|---|
| **Zhang (2008), *Artificial Intelligence* 172(16–17), 1873–1896** — the FCI orientation rules R0–R10 | Elsevier, not open access | DOI [10.1016/j.artint.2008.08.001](https://doi.org/10.1016/j.artint.2008.08.001). Semantic Scholar reports it as "bronze" OA at the publisher, so [the ScienceDirect page](https://www.sciencedirect.com/science/article/pii/S0004370208001008) may serve it free in a browser; otherwise use an institutional subscription, or the repository listings at [Lingnan](https://commons.ln.edu.hk/sw_master/732/) or [Caltech](https://authors.library.caltech.edu/12648/) |
| **Spirtes, Glymour & Scheines, *Causation, Prediction, and Search*, 2nd ed. (2000)** | Book | The canonical statement of PC and FCI. Colombo & Maathuis (2014) and Spirtes (2010) cover the same ground and are included |
| **Spirtes & Glymour (1991)**, the original PC paper | SAGE; the CMU KiltHub copy would not download | DOI [10.1177/089443939100900106](https://doi.org/10.1177/089443939100900106). Superseded for citation purposes by Colombo & Maathuis (2014), which states the PC skeleton search as numbered algorithms |

**Zhang (2008) is the one you will actually need.** `FunctionMapping.md` §6
anchors every FCI orientation rule to it by section, rule label, theorem and
definition number, so the mapping is checkable against a copy you obtain
yourself — but `src/search/fci_orient.cpp` cannot be derived from first
principles without it.

**Schwarz (1978)** is present but is a scanned image with no text layer, so it
can be cited bibliographically but not anchored to an equation. Ramsey (2015)
§4 is used as the citable proxy for the BIC formula as this port applies it.

---

## Shared foundations

**Meek, C. (1995).** Causal inference and causal explanation with background
knowledge. *Proceedings of the Eleventh Conference on Uncertainty in Artificial
Intelligence (UAI)*, 403–410.
· arXiv: [1302.4972](https://arxiv.org/abs/1302.4972)
· `papers/meek1995-orientation-rules.pdf`
· Underpins `src/search/meek_rules.cpp`, and the CPDAG conversion inside
  `fges.cpp`, `permutation_search.cpp` and `tests/simulation.py`.

**Schwarz, G. (1978).** Estimating the dimension of a model. *Annals of
Statistics*, 6(2), 461–464.
· DOI: [10.1214/aos/1176344136](https://doi.org/10.1214/aos/1176344136)
· `papers/schwarz1978-bic.pdf` (scanned; no text layer)
· The BIC behind `src/search/sem_bic_score.cpp`.

**Spirtes, P. (2010).** Introduction to causal inference. *Journal of Machine
Learning Research*, 11, 1643–1662.
· [jmlr.org/papers/volume11/spirtes10a](https://www.jmlr.org/papers/volume11/spirtes10a/spirtes10a.pdf)
· `papers/spirtes2010-intro-causal-inference.pdf`
· Survey; useful orientation, but contains no statement of the Fisher z test
  and no numbered PC pseudocode.

**Kalisch, M., Mächler, M., Colombo, D., Maathuis, M. H., & Bühlmann, P.
(2012).** Causal inference using graphical models with the R package pcalg.
*Journal of Statistical Software*, 47(11), 1–26.
· DOI: [10.18637/jss.v047.i11](https://doi.org/10.18637/jss.v047.i11)
· `papers/kalisch2012-pcalg-jss.pdf`
· Independent implementation of PC and FCI; its `doPdsep` option is the same
  variation axis as this port's omission of Possible-D-SEP (see GFCI below).

## PC and the adjacency search

**Colombo, D., & Maathuis, M. H. (2014).** Order-independent constraint-based
causal structure learning. *Journal of Machine Learning Research*, 15, 3741–3782.
· [jmlr.org/papers/volume15/colombo14a](https://www.jmlr.org/papers/volume15/colombo14a/colombo14a.pdf)
· `papers/colombo2014-pc-stable.pdf`
· The numbered statements of the PC skeleton search this port implements —
  Algorithm 3.2 (plain) and Algorithm 4.1 (stable) — behind `src/search/fas.cpp`
  and `src/search/pc.cpp`. Also the analysis explaining the hash-order
  sensitivities documented in `CLAUDE.md`.

**Ramsey, J., Zhang, J., & Spirtes, P. (2006).** Adjacency-faithfulness and
conservative causal inference. *Proceedings of the Twenty-Second Conference on
Uncertainty in Artificial Intelligence (UAI)*, 401–408.
· arXiv: [1206.6843](https://arxiv.org/abs/1206.6843)
· `papers/ramsey2006-conservative-pc.pdf`
· Source of the collider-orientation step and its conflict handling. Note that
  this port implements the plain rule, not the conservative one — see
  `FunctionMapping.md`.

**Raskutti, G., & Uhler, C. (2018).** Learning directed acyclic graph models
based on sparsest permutations. *Stat*, 7(1), e183.
· DOI: [10.1002/sta4.183](https://doi.org/10.1002/sta4.183)
· arXiv: [1307.0366](https://arxiv.org/abs/1307.0366)
· `papers/raskutti2018-sparsest-permutation.pdf`
· Section 3 is the only source in this collection that states the Fisher
  z-transform partial-correlation test implemented in
  `src/search/ind_test_fisher_z.cpp`. Also the sparsest-permutation theory
  BOSS and GRaSP rest on.

## FGES

**Chickering, D. M. (2002).** Optimal structure identification with greedy
search. *Journal of Machine Learning Research*, 3, 507–554.
· [jmlr.org/papers/volume3/chickering02b](https://www.jmlr.org/papers/volume3/chickering02b/chickering02b.pdf)
· `papers/chickering2002-ges.pdf`
· The GES paper. Definitions 12–14 and Theorems 15/17 with Corollaries 16/18
  are the Insert and Delete operators in `src/search/fges.cpp`; Lemma 10 is the
  optimality result. Its backward phase is also reused by
  `src/search/bes_permutation.cpp`.

**Ramsey, J. (2015).** Scaling up greedy causal search for continuous
variables. Technical Report, Center for Causal Discovery, Pittsburgh.
· arXiv: [1507.07749](https://arxiv.org/abs/1507.07749)
· `papers/ramsey2015-fgs-techreport.pdf`
· **Note on identity:** this is the technical report, titled "Scaling up Greedy
  Causal Search for Continuous Variables" and calling the algorithm FGS. It is
  the arXiv preprint behind Ramsey, Glymour, Sanchez-Romero & Glymour (2017),
  "A million variables and more", *International Journal of Data Science and
  Analytics*, 3(2), 121–129, DOI
  [10.1007/s41060-016-0032-z](https://doi.org/10.1007/s41060-016-0032-z), which
  is paywalled. Section numbers cited in `FunctionMapping.md` are the technical
  report's and will not match the published article.
· Source of the arrow caching, the effect-edge first pass, and the penalised
  BIC as Tetrad applies it. Several statements in it are internally
  inconsistent; `FunctionMapping.md` records which ones the code correctly
  ignores in favour of Chickering.

## FCI, MAGs and PAGs

**Spirtes, P., Meek, C., & Richardson, T. (1995).** Causal inference in the
presence of latent variables and selection bias. *Proceedings of the Eleventh
Conference on Uncertainty in Artificial Intelligence (UAI)*, 499–506.
· arXiv: [1302.4983](https://arxiv.org/abs/1302.4983)
· `papers/spirtes1995-fci-selection-bias.pdf`
· The UAI version of the FCI algorithm; the 1999 book chapter ("An algorithm
  for causal inference in the presence of latent variables and selection bias",
  in *Computation, Causation, and Discovery*, AAAI Press, 211–252) is the
  usually-cited form and is not open access.

**Richardson, T., & Spirtes, P. (2002).** Ancestral graph Markov models.
*Annals of Statistics*, 30(4), 962–1030.
· DOI: [10.1214/aos/1031689015](https://doi.org/10.1214/aos/1031689015)
· `papers/richardson2002-ancestral-graphs.pdf`
· Defines ancestral graphs, maximality and m-separation — what the endpoints in
  `src/graph/edge.h` mean. It does **not** discuss PAGs; for those use Zhang
  (2008, JMLR) below.

**Zhang, J. (2008).** Causal reasoning with ancestral graphs. *Journal of
Machine Learning Research*, 9, 1437–1474.
· [jmlr.org/papers/volume9/zhang08a](https://www.jmlr.org/papers/volume9/zhang08a/zhang08a.pdf)
· `papers/zhang2008-ancestral-reasoning.pdf`
· Definition 1 (MAG) and Definition 3 (PAG) — the semantics of what every FCI
  variant here returns — plus the uncovered-path and possibly-directed-path
  definitions used by R9 and R10. Explicitly excludes selection bias.

**Zhang, J. (2008).** On the completeness of orientation rules for causal
discovery in the presence of latent confounders and selection bias.
*Artificial Intelligence*, 172(16–17), 1873–1896.
· DOI: [10.1016/j.artint.2008.08.001](https://doi.org/10.1016/j.artint.2008.08.001)
· **Not bundled — not open access.** See "What is not here" above for ways to
  obtain it.
· The primary source for `src/search/fci_orient.cpp`: rules R0–R10 in §3.1–§3.2
  (printed as a lettered list inside algorithm steps F3/F4, not as numbered
  definitions), soundness in Theorem 1, and the completeness result Theorem 4
  that `setCompleteRuleSetUsed(true)` claims. Also Definition 7 (discriminating
  path), Definition 9 (uncovered path) and Definition 10 (potentially directed
  path), which R4, R5, R9 and R10 are stated in terms of.

**Wang, T., et al. (2024).** New rules for causal identification with background
knowledge. arXiv preprint.
· arXiv: [2407.15259](https://arxiv.org/abs/2407.15259)
· `papers/wang2024-new-rules-background-knowledge.pdf`
· Added as a substitute before Zhang (2008) was available; Appendix A.2
  restates R1–R4 and R8–R10 verbatim but omits R5–R7. Retained as an
  independent cross-check and for its treatment of background knowledge, but the
  mapping now anchors to Zhang directly.

**Colombo, D., Maathuis, M. H., Kalisch, M., & Richardson, T. S. (2012).**
Learning high-dimensional directed acyclic graphs with latent and selection
variables. *Annals of Statistics*, 40(1), 294–321.
· DOI: [10.1214/11-AOS940](https://doi.org/10.1214/11-AOS940)
· arXiv: [1104.5617](https://arxiv.org/abs/1104.5617)
· `papers/colombo2012-rfci.pdf`
· RFCI. Definition 3.1 (FCI-PAG), Definition 3.3 (Possible-D-SEP) and
  Lemma 3.2 (the discriminating-path rule) are the best available anchors for
  the corresponding parts of `fci_orient.cpp`.

## GFCI

**Ogarrio, J. M., Spirtes, P., & Ramsey, J. (2016).** A hybrid causal search
algorithm for latent variable models. *Proceedings of the Eighth International
Conference on Probabilistic Graphical Models (PGM)*, PMLR 52, 368–379.
· [proceedings.mlr.press/v52/ogarrio16](https://proceedings.mlr.press/v52/ogarrio16.pdf)
· `papers/ogarrio2016-gfci.pdf`
· Algorithm 1 is the pipeline in `src/search/gfci.cpp` and `src/search/star_fci.cpp`.
· **Important:** the published algorithm includes a Possible-D-SEP edge-removal
  step (step D) that Tetrad 7.6.3 and therefore this port omit, and that step is
  used in the proof of Theorem 7. See `FunctionMapping.md`.

## BOSS and GRaSP

**Andrews, B., Ramsey, J., Sánchez-Romero, R., Camchong, J., & Kummerfeld, E.
(2023).** Fast scalable and accurate discovery of DAGs using the best order
score search and grow-shrink trees. *Advances in Neural Information Processing
Systems 36 (NeurIPS)*.
· arXiv: [2310.17679](https://arxiv.org/abs/2310.17679)
· `papers/andrews2023-boss-gst.pdf`
· Algorithms 1–5 and Proposition 2 behind `src/search/boss.cpp`,
  `grow_shrink_tree.cpp` and `permutation_search.cpp`. Note the year: several
  headers in `src/search/` cite this as 2024.

**Lam, W.-Y., Andrews, B., & Ramsey, J. (2022).** Greedy relaxations of the
sparsest permutation algorithm. *Proceedings of the Thirty-Eighth Conference on
Uncertainty in Artificial Intelligence (UAI)*, PMLR 180, 1052–1062.
· [proceedings.mlr.press/v180/lam22a](https://proceedings.mlr.press/v180/lam22a/lam22a.pdf)
· `papers/lam2022-grasp.pdf`
· Definitions 4.1–4.3 (tuck, singular, ct-sequence) and Algorithms 1–2 behind
  `src/search/grasp.cpp` and `teyssier_scorer.cpp`.

**Teyssier, M., & Koller, D. (2005).** Ordering-based search: a simple and
effective algorithm for learning Bayesian networks. *Proceedings of the
Twenty-First Conference on Uncertainty in Artificial Intelligence (UAI)*, 584–590.
· arXiv: [1207.1429](https://arxiv.org/abs/1207.1429)
· `papers/teyssier2005-ordering-search.pdf`
· The ordering-space framing behind `src/search/teyssier_scorer.cpp`. Note the
  year: `teyssier_scorer.h` cites 2012.

**Margaritis, D., & Thrun, S. (1999).** Bayesian network induction via local
neighborhoods. *Advances in Neural Information Processing Systems 12 (NIPS)*,
505–511.
· [proceedings.neurips.cc/paper/1999](https://proceedings.neurips.cc/paper/1999/file/5d79099fcdf499f12b79770834c0164a-Paper.pdf)
· `papers/margaritis1999-grow-shrink.pdf`
· The grow-shrink procedure generalised by `src/search/grow_shrink_tree.cpp`.
  The extracted text is OCR-damaged around the algorithm listing; cite by
  figure/step, not by quotation.

---

## Reproducing this folder

PDFs were fetched with `curl -L` from the URLs above. To regenerate the plain
text used for verification (not committed):

```bash
mkdir -p References/.text
for f in References/papers/*.pdf; do
  pdftotext -q "$f" "References/.text/$(basename "${f%.pdf}").txt"
done
```
