# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### CI/CD

- Add automated GitHub Release creation on version tags


## [0.1.0] - 2026-06-05

### Features

- Initial monorepo setup with CI/CD, tooling, and project scaffolding

- Add CUDA infrastructure and GPU pipeline for MetroEffects

- Add metro_add_gpu_plugin() CMake function for CUDA plugin support

- Add 6 new plugins, CUDA pipeline, and 7 test suites for MET-59

- Add pre-commit hook configuration for Metro Design repos


### Documentation

- Add push-after-completion policy to CONTRIBUTING.md


### Chores

- Update LICENSE to proprietary, add DEPLOYMENT.md, add new screenshots

- Add copyright headers to all source files

- Enhance .gitignore with additional generated file patterns

- Add VR Teams plugin and improve cross-platform build config


### CI/CD

- Add repo health dashboard and branch lifecycle automation


### Bug Fixes

- Remove test artifacts from tracking, update .gitignore

