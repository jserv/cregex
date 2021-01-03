# cregex

`cregex` is a small implementation of regular expression matching engine in C.
It is based on two papers by Russ Cox:
* [Regular Expression Matching Can Be Simple And Fast](https://swtch.com/~rsc/regexp/regexp1.html)
* [Regular Expression Matching: the Virtual Machine Approach](https://swtch.com/~rsc/regexp/regexp2.html)

## Features

* `^` and `$` anchors
* `.` match any single character
* `[...]` and `[^...]` character classes
* `?`, `*`, `+`, and `{x,y}` greedy quantifiers
* `??`, `*?`, `+?`, and `{x,y}?` non-greedy quantifiers
* `(...)` capturing groups

## License

`cregex` is freely redistributable under the BSD 2 clause license.
Use of this source code is governed by a BSD-style license that can be found in the `LICENSE` file.
