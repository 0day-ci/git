@@
type T;
T dst;
T src;
@@
(
- memcpy(&dst, &src, sizeof(dst));
+ dst = src;
|
- memcpy(&dst, &src, sizeof(src));
+ dst = src;
|
- memcpy(&dst, &src, sizeof(T));
+ dst = src;
)

@@
type T;
T dst;
T *src;
@@
(
- memcpy(&dst, src, sizeof(dst));
+ dst = *src;
|
- memcpy(&dst, src, sizeof(*src));
+ dst = *src;
|
- memcpy(&dst, src, sizeof(T));
+ dst = *src;
)
