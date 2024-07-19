Contributing to this project
============================

First and foremost, thank you for the contribution! No matter what it is, a
simple typo fix, a significant feature or a request for documentation, we value
your effort a lot and want to thank you upfront.

The following guidelines should help you to improve the codebase. Note the
wording "guidelines", not "rules", and use your best judgment.

## Code contribution steps

1. Fork the Project
2. Create your Feature Branch (``git checkout -b feature/AmazingFeature``)
3. Commit your Changes (``git commit -m 'Add some AmazingFeature'``)
4. Push to the Branch (``git push origin feature/AmazingFeature``)
5. Open a Pull Request

## General

- Please write tests, especially when introducing new functionality or fixing
  bugs.
- Make sure you run all tests and read the compile-time and run-time warnings.
- Think about whether it would be better to document new code. When unsure,
  stay on the side of "write documentation". Specifically, code should be
  documented if for some reason (optimization, delicate concurrency behaviour
  etc.) it's not clear _how_ it does what it does. The _what_ should be
  derivable from class, method and variable names; if not, consider writing
  short documentation.
- Please stick to our coding style.

## Commit message style

We welcome commit messages that explain the changes, the reasoning behind them
and what parts of the system are affected. [Conventional
Commits](https://www.conventionalcommits.org/en/v1.0.0/) are close to the
targeted style, but deviations are possible.

## Issues and feature requests

We welcome new ideas, improvements and features. Please check first if a
suggested improvement/PR already exists on the tracker. For both issues and
PRs, the best way to communicate your wishes is to explain the use case and
motivation first and the suggested steps afterwards.
