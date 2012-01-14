#include <verifier-builtins.h>

void *dm_malloc_aux(size_t s, const char *file, int line)
{
    (void) file;
    (void) line;
    return malloc(s);
}

void *dm_zalloc_aux(size_t s, const char *file, int line)
{
    (void) file;
    (void) line;
    return calloc(s, 1);
}

char *__strdup (__const char *__string)
{
    size_t len = 1UL + strlen(__string);
    char *dup = malloc(len);
    memmove(dup, __string, len);
    return dup;
}

char *strncpy (char *__restrict __dest,
        __const char *__restrict __src, size_t __n)
{
    return memcpy(__dest, __src, __n);
}

int strncmp (__const char *__s1, __const char *__s2, size_t __n)
{
    strlen(__s1);
    strlen(__s2);
    (void) __n;
    return ___sl_get_nondet_int();
}

int is_orphan_vg(const char *vg_name)
{
    strlen(vg_name);
    return ___sl_get_nondet_int();
}

/* verbatim copy from uuid-prep.c */
int id_write_format(const struct id *id, char *buffer, size_t size)
{
 int i, tot;

 static const unsigned group_size[] = { 6, 4, 4, 4, 4, 4, 6 };

 ((32 == 32) ? (void) (0) : __assert_fail ("32 == 32", "uuid/uuid.c", 163, __PRETTY_FUNCTION__));


 if (size < (32 + 6 + 1)) {
  print_log(3, "uuid/uuid.c", 167 , -1,"Couldn't write uuid, buffer too small.");
  return 0;
 }

 for (i = 0, tot = 0; i < 7; i++) {
  memcpy(buffer, id->uuid + tot, group_size[i]);
  buffer += group_size[i];
  tot += group_size[i];
  *buffer++ = '-';
 }

 *--buffer = '\0';
 return 1;
}

#define NULL ((void *)0)

/*
 * Given the address v of an instance of 'struct dm_list' called 'head' 
 * contained in a structure of type t, return the containing structure.
 */
#define dm_list_struct_base(v, t, head) \
    ((t *)((const char *)(v) - (const char *)&((t *) 0)->head))

/*
 * Given the address v of an instance of 'struct dm_list list' contained in
 * a structure of type t, return the containing structure.
 */
#define dm_list_item(v, t) dm_list_struct_base((v), t, list)

/*
 * Given the address v of one known element e in a known structure of type t,
 * return another element f.
 */
#define dm_struct_field(v, t, e, f) \
    (((t *)((uintptr_t)(v) - (uintptr_t)&((t *) 0)->e))->f)

/*
 * Given the address v of a known element e in a known structure of type t,
 * return the list head 'list'
 */
#define dm_list_head(v, t, e) dm_struct_field(v, t, e, list)

/* overridden simplified implementation of dm_hash_create() */
struct dm_hash_table *dm_hash_create(unsigned size_hint)
{
    (void) size_hint;

    void *ptr = malloc(sizeof(struct dm_list));
    if (!ptr)
        abort();

    /* we approximate hash table using an unordered list */
    dm_list_init(ptr);
    return ptr;
}

struct ht_node {
    void *data;
    struct dm_list list;
};

/* overridden simplified implementation of dm_hash_lookup() */
void *dm_hash_lookup(struct dm_hash_table *t, const char *key)
{
    struct dm_list *head = (struct dm_list *) t;
    struct dm_list *pos;
    (void) key;

    /* return either random list node or NULL */
    for (pos = head->n; head != pos; pos = pos->n)
        if (___sl_get_nondet_int())
            return dm_list_item(pos, struct ht_node)->data;

    return NULL;
}

/* overridden simplified implementation of dm_hash_insert() */
int dm_hash_insert(struct dm_hash_table *t, const char *key, void *data)
{
    void *head = t;
    struct dm_list *pos = head;
    (void) key;

    /* seek random list position */
    while (___sl_get_nondet_int())
        pos = pos->p;

    if (head != pos && ___sl_get_nondet_int())
        /* simulate lookup success if we have at least one node in the list */
        return 0;

    /* allocate a new node */
    struct ht_node *node = malloc(sizeof *node);
    if (!node)
        return 0;

    node->data = data;

    /* insert the new node at a random position in the list */
    dm_list_add(pos, &node->list);
    return 1;
}

/* overridden simplified implementation of dm_hash_remove() */
void dm_hash_remove(struct dm_hash_table *t, const char *key)
{
    void *head = t;
    struct dm_list *pos = head;
    (void) key;

    /* seek random list position */
    while (___sl_get_nondet_int())
        pos = pos->p;

    if (head == pos)
        /* not found*/
        return;

    dm_list_del(pos);
    free(dm_list_item(pos, struct ht_node));
}

