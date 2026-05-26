#include "kvdb_ctrl.h"
#include "flashdb.h"
#include "variables.h"
#include <string.h>

typedef struct {
    const char *key;
    void       *ptr;
    uint8_t     size;
} persist_entry_t;

#define KV(name, type) {#name, (void *)&name, sizeof(name)},
static const persist_entry_t persist_entries[] = { PERSIST_LIST };
#undef KV

#define DIRTY_WORDS ((PERSIST_COUNT + 31) / 32)
static uint32_t persist_dirty[DIRTY_WORDS];

void kvdb_persist_load(void)
{
    if (!kvdb.parent.init_ok) return;

    for (int i = 0; i < PERSIST_COUNT; i++) {
        struct fdb_blob blob;
        size_t len = fdb_kv_get_blob(&kvdb, persist_entries[i].key,
                                      fdb_blob_make(&blob, persist_entries[i].ptr, persist_entries[i].size));
        (void)len;
    }
}

void kvdb_persist_mark(int index)
{
    if (index >= 0 && index < PERSIST_COUNT) {
        GLOBAL(persist_dirty[index / 32] |= (1U << (index % 32)));
    }
}

void kvdb_persist_flush(void)
{
    if (!kvdb.parent.init_ok) return;

    uint32_t dirty[DIRTY_WORDS];
    GLOBAL(
        memcpy(dirty, persist_dirty, sizeof(dirty));
        memset(persist_dirty, 0, sizeof(persist_dirty));
    );

    for (int i = 0; i < PERSIST_COUNT; i++) {
        if (dirty[i / 32] & (1U << (i % 32))) {
            const persist_entry_t *e = &persist_entries[i];
            struct fdb_blob blob;
            fdb_err_t ret = fdb_kv_set_blob(&kvdb, e->key,
                            fdb_blob_make(&blob, e->ptr, e->size));
            if (ret != FDB_NO_ERR) {
                GLOBAL(persist_dirty[i / 32] |= (1U << (i % 32)));
            }
        }
    }
}
