// check value pointed to by 'ptr'; if it is still equal to the
// value passed in by 'old', then atomically swap in the 'new' value
int
compareandswap(int *ptr, int old, int new)
{
    unsigned char ret;

    // Note that sete sets a 'byte' not the word 

    __asm__ __volatile__ (
        "  lock\n"
        "  cmpxchgl %2,%1\n"
        "  sete %0\n"
        : "=q" (ret), "=m" (*ptr)
        : "r" (new), "m" (*ptr), "a" (old)
        : "memory");

    return ret;
}

