/*
    THOSE WILL BE IN THE PYTHON'S MAIN
*/

void* dedipy_calloc (size_t n, size_t size_);
void dedipy_free (void* const data);
void* dedipy_malloc (size_t size_);
void* dedipy_realloc (void* const data_, const size_t dataSizeNew_);
void* dedipy_reallocarray (void *ptr, size_t nmemb, size_t size);
void dedipy_main (void);
