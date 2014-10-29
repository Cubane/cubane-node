#ifndef LOOKUP2_H
#define LOOKUP2_H

typedef uint32_t ub4;   /* unsigned 4-byte quantities */
typedef uint8_t  ub1;

#define hashsize(n) ((ub4)1<<(n))
#define hashmask(n) (hashsize(n)-1)

ub4 hash3( const ub1 * k, ub4 length, ub4 initval );

#endif /* LOOKUP2_H */
