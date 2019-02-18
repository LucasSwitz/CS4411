#undef malloc
#undef calloc
#undef free
#undef realloc

#define malloc(s)		m_alloc((s))
#define calloc(n, s)	m_calloc((n), (s))
#define free(p)			m_free((p))
#define realloc(p, s)	m_realloc((p), (s))

void *m_alloc(int size);
void *m_calloc(int nitems, int size);
void m_free(void *ptr);
void *m_realloc(void *ptr, int size);
