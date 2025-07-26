# vitaprof

A PS Vita profiling library build on vitasdk

EXPERIMENTAL : Not yet fully working (gmon.out is unusable)

## Note

libc_p is necessary when enabling profiling support, so fake it by creating a symlink :
`ln -s $VITASDK/arm-vita-eabi/lib/libc.a $VITASDK/arm-vita-eabi/lib/libc_p.a`
This does not really hurt us because it only means we do not have profiling in the standard
library which is fine.

## Usage

TODO
