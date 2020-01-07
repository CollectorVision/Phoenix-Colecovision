// these functions currently live in crt0_bios.s

void *memset (void *buf, unsigned int ch, unsigned int count);
void *memcpy (void *dst, void *src, unsigned int count);
void *memmove (void *dst, void *src, unsigned int count);
