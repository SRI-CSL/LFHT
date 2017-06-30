# An Expanding (Almost) Lock Free Hash Table


```
make
```

followed by

```
make check
```

to stress the poor wee thing

```
make stress
```

takes about a week.


The algorithm we use is inspired by 
[Concurrent Hash Tables: Fast and General?(!) version 2](doc/concurrent_htables-v2.pdf)[arxiv](https://arxiv.org/abs/1601.04017),
but is different in several important aspects. The most noticable difference
is that we do not mark the keys before they are copied, but rather require the
threads to copy before marking.
