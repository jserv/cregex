# cregex

`cregex` is a compact implementation of [regular expression](https://en.wikipedia.org/wiki/Regular_expression)
(regex) matching engine in C. Its design was inspired by Rob Pike's regex-code for the book "Beautiful Code"
[available online here](https://www.cs.princeton.edu/courses/archive/spr09/cos333/beautiful.html).
It is based on two papers by Russ Cox:
* [Regular Expression Matching Can Be Simple And Fast](https://swtch.com/~rsc/regexp/regexp1.html)
* [Regular Expression Matching: the Virtual Machine Approach](https://swtch.com/~rsc/regexp/regexp2.html)

`cregex` supports a subset of the syntax and semantics of the [POSIX Basic Regular Expressions](https://www.regular-expressions.info/posix.html).
The main design goal of `cregex` is to be small, correct, self contained and
use few resources while retaining acceptable performance and feature completeness.

## Features

* `^` and `$` anchors
* `.` match any single character
* `[...]` and `[^...]` character classes
* `?`, `*`, `+`, and `{x,y}` greedy quantifiers
* `??`, `*?`, `+?`, and `{x,y}?` non-greedy quantifiers
* `(...)` capturing groups

## Build and Test

Simply run to build the library and test programs.
```shell
$ make
```

Run the tests from Go distribution.
```shell
$ make check
```

Visualize the regular expressions with [Graphviz](https://graphviz.org/).
```shell
$ tests/re2dot "(a*)(b{0,1})(b{1,})b{3}" | dot -Tpng -o out.png
```

## License

`cregex` is freely redistributable under the BSD 2 clause license.
Use of this source code is governed by a BSD-style license that can be found in the `LICENSE` file.
