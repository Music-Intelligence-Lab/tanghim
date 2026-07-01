---
title: Tuning Systems and Starting Note Names
description: How to choose a tuning system and what starting note names mean.
---

A tuning system (tanghīm) is an ordered sequence of pitch classes within an octave. The tuning system selector lets you browse all available systems from the DiArMaqAr API. The plugin automatically checks for new data when it loads; if updates are available, the **Check for Updates** button in the status bar turns gold.

## Starting Note Names

Each tuning system has one or more **starting note names** — the name for the foundational pitch from which the tuning begins. Starting note names are not arbitrary transpositions; they reflect the historical and practical origins of each system:

- **Oud-based systems** (e.g. al-Kindī, al-Fārābī, Ibn Sīnā) typically start on **ʿushayrān**, reflecting oud tuning in perfect fourths.
- **Monochord and sonometer systems** (e.g. Cairo Congress 1932) typically start on **yegāh** or **rāst**, reflecting theoretical measurement approaches.

The starting note name matters because it determines the available maqāmāt, transposition possibilities, and modulation characteristics of the system. Changing the starting note name changes which maqamat are available and how they can be transposed — it is functionally the same as switching to a different tuning system entirely.

## Caching

The plugin caches tuning data locally, so internet access is only needed the first time you select a particular tuning system and starting note name combination. Green tick indicators in the maqam selector show which combinations have already been cached.
