# An Expanding (Almost) Lock Free Hash Table

###  Prerequisites

The software should build out of the box on any `x86_64` based machine with a modern C
compiler. We rely on the [standard atomics library](http://en.cppreference.com/w/c/atomic)
that is part of `C11`. We have tested the software as thoroughly as we could on both
Darwin and Linux.

### Building

```
make
```

followed by

```
make check
```

should do some rudimentary tests.

```
make stress
```

is a much more strenuous set of tests. Be patient, it takes about a week.


The algorithm we use is inspired by a paper by [Tobias Maier](doc/concurrent_htables-v2.pdf), [arxiv](https://arxiv.org/abs/1601.04017),
but is different in several important aspects. The most noticable difference
is that we do not mark the keys before they are copied, but rather require the
threads to copy before marking.
