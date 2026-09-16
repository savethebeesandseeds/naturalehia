# Cleaning the Brahmaputra

A [Waajacu](https://waajacu.com/) research project by **Santiago Restrepo**.

The paper and poster describe an exploratory workflow for screening protein
structures against pollutants, using public molecular databases and GNINA.
Docking scores are screening signals; they do not establish pollutant degradation
or catalytic activity. The folder contains research documents and illustrations,
not an implemented screening pipeline or experimentally validated results.

## Documents

- `project_presentation.tex` and `project_presentation.pdf`: the research paper,
  retaining its IEEE two-column format and workflow diagram.
- `targeting_pollution_poster.tex` and `targeting_pollution_poster.pdf`: the poster.
- `atrazine.png`, `4v1y.png`, `central.png`, and `central_transparent.png`:
  scientific illustrations.

## Build

From this directory, using [Tectonic](https://tectonic-typesetting.github.io/):

```sh
tectonic --untrusted project_presentation.tex
tectonic --untrusted targeting_pollution_poster.tex
```

The PDFs are written alongside their sources. Both documents were rebuilt with
Tectonic 0.17.0. Generated LaTeX auxiliary files are ignored by Git.
