int cygwin_access(const char *filename, int mode);
#undef access
#define access cygwin_access


int cygwin_offset_1st_component(const char *path);
#define offset_1st_component cygwin_offset_1st_component
