#include "builtin.h"
#include "cache.h"
#include "config.h"
#include "dir.h"
#include "git-compat-util.h"
#include "lockfile.h"
#include "packfile.h"
#include "parse-options.h"
#include "midx.h"

static char const * const builtin_midx_usage[] = {
	N_("git midx --write [--pack-dir <packdir>]"),
	NULL
};

static struct opts_midx {
	const char *pack_dir;
	int write;
} opts;

static int build_midx_from_packs(
	const char *pack_dir,
	const char **pack_names, uint32_t nr_packs,
	const char **midx_id)
{
	struct packed_git **packs;
	const char **installed_pack_names;
	uint32_t i, j, nr_installed_packs = 0;
	uint32_t nr_objects = 0;
	struct pack_midx_entry *objects;
	struct pack_midx_entry **obj_ptrs;
	uint32_t nr_total_packs = nr_packs;
	uint32_t pack_offset = 0;
	struct strbuf pack_path = STRBUF_INIT;
	int baselen;

	if (!nr_total_packs) {
		*midx_id = NULL;
		return 0;
	}

	ALLOC_ARRAY(packs, nr_total_packs);
	ALLOC_ARRAY(installed_pack_names, nr_total_packs);

	strbuf_addstr(&pack_path, pack_dir);
	strbuf_addch(&pack_path, '/');
	baselen = pack_path.len;
	for (i = 0; i < nr_packs; i++) {
		strbuf_setlen(&pack_path, baselen);
		strbuf_addstr(&pack_path, pack_names[i]);

		strbuf_strip_suffix(&pack_path, ".pack");
		strbuf_addstr(&pack_path, ".idx");

		packs[nr_installed_packs] = add_packed_git(pack_path.buf, pack_path.len, 0);

		if (packs[nr_installed_packs] != NULL) {
			if (open_pack_index(packs[nr_installed_packs]))
				continue;

			nr_objects += packs[nr_installed_packs]->num_objects;
			installed_pack_names[nr_installed_packs] = pack_names[i];
			nr_installed_packs++;
		}
	}
	strbuf_release(&pack_path);

	if (!nr_objects || !nr_installed_packs) {
		FREE_AND_NULL(packs);
		FREE_AND_NULL(installed_pack_names);
		*midx_id = NULL;
		return 0;
	}

	ALLOC_ARRAY(objects, nr_objects);
	nr_objects = 0;

	for (i = pack_offset; i < nr_installed_packs; i++) {
		struct packed_git *p = packs[i];

		for (j = 0; j < p->num_objects; j++) {
			struct pack_midx_entry entry;

			if (!nth_packed_object_oid(&entry.oid, p, j))
				die("unable to get sha1 of object %u in %s",
				    i, p->pack_name);

			entry.pack_int_id = i;
			entry.offset = nth_packed_object_offset(p, j);

			objects[nr_objects] = entry;
			nr_objects++;
		}
	}

	ALLOC_ARRAY(obj_ptrs, nr_objects);
	for (i = 0; i < nr_objects; i++)
		obj_ptrs[i] = &objects[i];

	*midx_id = write_midx_file(pack_dir, NULL,
		installed_pack_names, nr_installed_packs,
		obj_ptrs, nr_objects);

	FREE_AND_NULL(packs);
	FREE_AND_NULL(installed_pack_names);
	FREE_AND_NULL(obj_ptrs);
	FREE_AND_NULL(objects);

	return 0;
}

static int midx_write(void)
{
	const char **pack_names = NULL;
	uint32_t i, nr_packs = 0;
	const char *midx_id = 0;
	DIR *dir;
	struct dirent *de;

	dir = opendir(opts.pack_dir);
	if (!dir) {
		error_errno("unable to open object pack directory: %s",
			    opts.pack_dir);
		return 1;
	}

	nr_packs = 256;
	ALLOC_ARRAY(pack_names, nr_packs);

	i = 0;
	while ((de = readdir(dir)) != NULL) {
		if (is_dot_or_dotdot(de->d_name))
			continue;

		if (ends_with(de->d_name, ".pack")) {
			ALLOC_GROW(pack_names, i + 1, nr_packs);
			pack_names[i++] = xstrdup(de->d_name);
		}
	}

	nr_packs = i;
	closedir(dir);

	if (!nr_packs)
		goto cleanup;

	if (build_midx_from_packs(opts.pack_dir, pack_names, nr_packs, &midx_id))
		die("failed to build MIDX");

	if (midx_id == NULL)
		goto cleanup;

	printf("%s\n", midx_id);

cleanup:
	if (pack_names)
		FREE_AND_NULL(pack_names);
	return 0;
}

int cmd_midx(int argc, const char **argv, const char *prefix)
{
	static struct option builtin_midx_options[] = {
		{ OPTION_STRING, 'p', "pack-dir", &opts.pack_dir,
			N_("dir"),
			N_("The pack directory containing set of packfile and pack-index pairs.") },
		OPT_BOOL('w', "write", &opts.write,
			N_("write midx file")),
		OPT_END(),
	};

	if (argc == 2 && !strcmp(argv[1], "-h"))
		usage_with_options(builtin_midx_usage, builtin_midx_options);

	git_config(git_default_config, NULL);
	if (!core_midx)
		die(_("git-midx requires core.midx=true"));

	argc = parse_options(argc, argv, prefix,
			     builtin_midx_options,
			     builtin_midx_usage, 0);

	if (!opts.pack_dir) {
		struct strbuf path = STRBUF_INIT;
		strbuf_addstr(&path, get_object_directory());
		strbuf_addstr(&path, "/pack");
		opts.pack_dir = strbuf_detach(&path, NULL);
	}

	if (opts.write)
		return midx_write();

	usage_with_options(builtin_midx_usage, builtin_midx_options);
	return 0;
}
