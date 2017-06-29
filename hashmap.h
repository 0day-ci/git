#ifndef HASHMAP_H
#define HASHMAP_H

/*
 * Generic implementation of hash-based key-value mappings.
 *
 */

/*
 * Ready-to-use hash functions for strings, using the FNV-1 algorithm (see
 * http://www.isthe.com/chongo/tech/comp/fnv).
 * `strhash` and `strihash` take 0-terminated strings, while `memhash` and
 * `memihash` operate on arbitrary-length memory.
 * `strihash` and `memihash` are case insensitive versions.
 * `memihash_cont` is a variant of `memihash` that allows a computation to be
 * continued with another chunk of data.
 */
extern unsigned int strhash(const char *buf);
extern unsigned int strihash(const char *buf);
extern unsigned int memhash(const void *buf, size_t len);
extern unsigned int memihash(const void *buf, size_t len);
extern unsigned int memihash_cont(unsigned int hash_seed, const void *buf, size_t len);

static inline unsigned int sha1hash(const unsigned char *sha1)
{
	/*
	 * Converts a cryptographic hash (e.g. SHA-1) into an int-sized hash code
	 * for use in hash tables. Cryptographic hashes are supposed to have
	 * uniform distribution, so in contrast to `memhash()`, this just copies
	 * the first `sizeof(int)` bytes without shuffling any bits. Note that
	 * the results will be different on big-endian and little-endian
	 * platforms, so they should not be stored or transferred over the net.
	 */

	/*
	 * Equivalent to 'return *(unsigned int *)sha1;', but safe on
	 * platforms that don't support unaligned reads.
	 */
	unsigned int hash;
	memcpy(&hash, sha1, sizeof(hash));
	return hash;
}

/*
 * struct hashmap_entry is an opaque structure representing an entry in the
 * hash table, which must be used as first member of user data structures.
 * Ideally it should be followed by an int-sized member to prevent unused
 * memory on 64-bit systems due to alignment.
 */
struct hashmap_entry {
	/*
	 * next points to the next entry in case of collisions (i.e. if
	 * multiple entries map to the same bucket)
	 */
	struct hashmap_entry *next;

	/* entry's hash code */
	unsigned int hash;
};

/*
 * User-supplied function to test two hashmap entries for equality. Shall
 * return 0 if the entries are equal.
 *
 * This function is always called with non-NULL `entry` and `entry_or_key`
 * parameters that have the same hash code.
 *
 * When looking up an entry, the `key` and `keydata` parameters to hashmap_get
 * and hashmap_remove are always passed
 * as second `entry_or_key` and third argument `keydata`, respectively.
 * Otherwise, `keydata` is NULL.
 *
 * There are two modes for this function to be used:
 * (a) When looking for an exact match of a given `entry`, then `keydata`
 *     ought to be NULL, and this function should cast `entry` and
 *     `entry_or_key` to the user entries and check for equality.
 * (b) When it is too expensive to allocate such a user entry (either because
 *     it is large or varialbe sized, such that it is not on the stack),
 *     Then the relevant data to check for equality should be passed via
 *     ` keydata`.
 *
 * Resulting from these modes, this function should compare `keydata` to `entry`
 * when `keydata` is not NULL. `entry_or_key` may be a bogus hashmap_entry (see
 * hashmap_get_from_hash).
 *
 * When `keydata` is NULL,`entry_or_key` is guaranteed to be a valid user supplied
 * entry, such that the comparision should be made between `entry` and
 * `entry_or_key`, both cast to the user type.
 *
 * The `data` entry is the pointer given in the init function and is always
 * given.
 */
typedef int (*hashmap_cmp_fn)(const void *entry, const void *entry_or_key,
		const void *keydata, const void *data);

/*
 * struct hashmap is the hash table structure. Members can be used as follows,
 * but should not be modified directly.
 */
struct hashmap {
	struct hashmap_entry **table;

	/* Stores the comparison function specified in `hashmap_init()`. */
	hashmap_cmp_fn cmpfn;
	const void *data;

	/* total number of entries (0 means the hashmap is empty) */
	unsigned int size;

	/*
	 * tablesize is the allocated size of the hash table. A non-0 value
	 * indicates that the hashmap is initialized. It may also be useful
	 * for statistical purposes (i.e. `size / tablesize` is the current
	 * load factor).
	 */
	unsigned int tablesize;

	unsigned int grow_at;
	unsigned int shrink_at;

	/* Prevent automatic rehashes during inserts and deletes when set. */
	unsigned disallow_rehash : 1;
};

/* hashmap functions */

/*
 * Initializes a hashmap structure.
 *
 * `map` is the hashmap to initialize.
 *
 * The `equals_function` can be specified to compare two entries for equality.
 * If NULL, entries are considered equal if their hash codes are equal.
 *
 * When the compare function needs specific user supplied `data`, it should
 * be given in `data`. It will be passed to the compare function in each call.
 *
 * If the total number of entries is known in advance, the `initial_size`
 * parameter may be used to preallocate a sufficiently large table and thus
 * prevent expensive resizing. If 0, the table is dynamically resized.
 */
extern void hashmap_init(struct hashmap *map,
			 hashmap_cmp_fn equals_function,
			 const void *data,
			 size_t initial_size);

/*
 * Frees a hashmap structure and allocated memory.
 *
 * If `free_entries` is true, each hashmap_entry in the map is freed as well.
 */
extern void hashmap_free(struct hashmap *map, int free_entries);

/* hashmap_entry functions */

/*
 * Initializes a hashmap_entry structure.
 *
 * `entry` points to the entry to initialize.
 * `hash` is the hash code of the entry.
 *
 * The hashmap_entry structure does not hold references to external resources,
 * and it is safe to just discard it once you are done with it (i.e. if
 * your structure was allocated with xmalloc(), you can just free(3) it,
 * and if it is on stack, you can just let it go out of scope).
 */
static inline void hashmap_entry_init(void *entry, unsigned int hash)
{
	struct hashmap_entry *e = entry;
	e->hash = hash;
	e->next = NULL;
}

/*
 * Returns the hashmap entry for the specified key, or NULL if not found.
 *
 * `map` is the hashmap structure.
 * `key` is a user data structure that starts with hashmap_entry that has at
 *  least been initialized with the proper hash code (via `hashmap_entry_init`).
 * `keydata` is a data structure that holds just enough information to check
 * for equality to a given entry.
 *
 * One of `key` or `keydata` shall be NULL.
 *
 * If an entry with matching hash code is found, `key` and `keydata` are passed
 * to `hashmap_cmp_fn` to decide whether the entry matches the key.
 */
extern void *hashmap_get(const struct hashmap *map, const void *key,
			 const void *keydata);

/*
 * Returns the next equal hashmap entry, or NULL if not found. This can be
 * used to iterate over duplicate entries (see `hashmap_add`).
 *
 * `map` is the hashmap structure.
 * `entry` is the hashmap_entry to start the search from, obtained via a previous
 * call to `hashmap_get` or `hashmap_get_next`.
 */
extern void *hashmap_get_next(const struct hashmap *map, const void *entry);

/*
 * Adds a hashmap entry. This allows to add duplicate entries (i.e.
 * separate values with the same key according to hashmap_cmp_fn).
 *
 * `map` is the hashmap structure.
 * `entry` is the entry to add.
 */
extern void hashmap_add(struct hashmap *map, void *entry);

/*
 * Adds or replaces a hashmap entry. If the hashmap contains duplicate
 * entries equal to the specified entry, only one of them will be replaced.
 *
 * `map` is the hashmap structure.
 * `entry` is the entry to add or replace.
 * Returns the replaced entry, or NULL if not found (i.e. the entry was added).
 */
extern void *hashmap_put(struct hashmap *map, void *entry);

/*
 * Removes a hashmap entry matching the specified key. If the hashmap
 * contains duplicate entries equal to the specified key, only one of
 * them will be removed.
 *
 * `map` is the hashmap structure.
 * `key` is a user data structure that starts with hashmap_entry that has at
 *  least been initialized with the proper hash code (via `hashmap_entry_init`).
 * `keydata` is a data structure that holds just enough information to check
 * for equality to a given entry.
 *
 * One of `key` or `keydata` shall be NULL.
 *
 * If an entry with matching hash code is found, `key` and `keydata` are
 * passed to `hashmap_cmp_fn` to decide whether the entry matches the key.
 *
 * Returns the removed entry, or NULL if not found.
 */
extern void *hashmap_remove(struct hashmap *map, const void *key,
		const void *keydata);

/*
 * Returns the hashmap entry for the specified hash code and key data,
 * or NULL if not found.
 *
 * `map` is the hashmap structure.
 * `hash` is the hash code of the entry to look up.
 *
 * If an entry with matching hash code is found, `keydata` is passed to
 * `hashmap_cmp_fn` to decide whether the entry matches the key. The
 * `entry_or_key` parameter points to a bogus hashmap_entry structure that
 * should not be used in the comparison.
 */
static inline void *hashmap_get_from_hash(const struct hashmap *map,
					  unsigned int hash,
					  const void *keydata)
{
	struct hashmap_entry key;
	hashmap_entry_init(&key, hash);
	return hashmap_get(map, &key, keydata);
}

/*
 * Returns the `bucket` an entry is stored in.
 * Useful for multithreaded read access.
 */
int hashmap_bucket(const struct hashmap *map, unsigned int hash);

/*
 * Disallow/allow rehashing of the hashmap.
 * This is useful if the caller knows that the hashmap
 * needs multi-threaded access.  The caller is still
 * required to guard/lock searches and inserts in a
 * manner appropriate to their usage.  This simply
 * prevents the table from being unexpectedly re-mapped.
 *
 * It is up to the caller to ensure that the hashmap is
 * initialized to a reasonable size to prevent poor
 * performance.
 *
 * When value=1, prevent future rehashes on adds and deleted.
 * When value=0, allow future rehahses.  This DOES NOT force
 * a rehash now.
 */
static inline void hashmap_disallow_rehash(struct hashmap *map, unsigned value)
{
	map->disallow_rehash = value;
}


/*
 * hashmap_iter functions
 *
 * Used to iterate over all entries of a hashmap. Note that it is
 * not safe to add or remove entries to the hashmap while
 * iterating.
 */

struct hashmap_iter {
	struct hashmap *map;
	struct hashmap_entry *next;
	unsigned int tablepos;
};

/* Initializes a `hashmap_iter` structure. */
extern void hashmap_iter_init(struct hashmap *map, struct hashmap_iter *iter);

/* Returns the next hashmap_entry, or NULL if there are no more entries. */
extern void *hashmap_iter_next(struct hashmap_iter *iter);

/* Initializes the iterator and returns the first entry, if any). */
static inline void *hashmap_iter_first(struct hashmap *map,
		struct hashmap_iter *iter)
{
	hashmap_iter_init(map, iter);
	return hashmap_iter_next(iter);
}

/* String interning */

/*
 * Returns the unique, interned version of the specified string or data,
 * similar to the `String.intern` API in Java and .NET, respectively.
 * Interned strings remain valid for the entire lifetime of the process.
 *
 * Can be used as `[x]strdup()` or `xmemdupz` replacement, except that interned
 * strings / data must not be modified or freed.
 *
 * Interned strings are best used for short strings with high probability of
 * duplicates.
 *
 * Uses a hashmap to store the pool of interned strings.
 */
extern const void *memintern(const void *data, size_t len);
static inline const char *strintern(const char *string)
{
	return memintern(string, strlen(string));
}

#endif
