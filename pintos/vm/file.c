/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"

static bool file_backed_swap_in(struct page *page, void *kva);
static bool file_backed_swap_out(struct page *page);
static void file_backed_destroy(struct page *page);

#include "threads/mmu.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
#include "vm/file.h"

/* DO NOT MODIFY this struct */
static const struct page_operations file_ops = {
    .swap_in = file_backed_swap_in,
    .swap_out = file_backed_swap_out,
    .destroy = file_backed_destroy,
    .type = VM_FILE,
};

/* The initializer of file vm */
void vm_file_init(void) {}

/* Initialize the file backed page */
bool file_backed_initializer(struct page *page, enum vm_type type, void *kva) {
  ASSERT(VM_TYPE(type) == VM_FILE);

  /* Set up the handler */
  page->operations = &file_ops;

  struct segment_aux *aux = (struct segment_aux *)page->uninit.aux;
  ASSERT(aux != NULL);
  page->uninit.aux = NULL;  // 소유권 이전

  struct file_page *file_page = &page->file;
  file_page->file = aux->file;
  file_page->ofs = aux->ofs;
  file_page->read_bytes = aux->read_bytes;
  file_page->zero_bytes = aux->zero_bytes;
  page->writable = aux->writable;

  return true;
}

/* Swap in the page by read contents from the file. */
static bool file_backed_swap_in(struct page *page, void *kva) {
  struct file_page *file_page UNUSED = &page->file;
  off_t file_size =
      file_read_at(file_page->file, kva, file_page->read_bytes, file_page->ofs);
  if (file_size != file_page->read_bytes) return false;
  if (file_page->zero_bytes > 0) {
    memset(kva + file_page->read_bytes, 0, file_page->zero_bytes);
  }
  return true;
}

/* Swap out the page by writeback contents to the file. */
static bool file_backed_swap_out(struct page *page) {
  struct file_page *file_page UNUSED = &page->file;

  bool is_dirty = pml4_is_dirty(page->owner->pml4, page->va);
  if (is_dirty) {
    off_t rewrite_file = file_write_at(file_page->file, page->frame->kva,
                                       file_page->read_bytes, file_page->ofs);
    if (rewrite_file != file_page->read_bytes) return false;
  }
  do_munmap(page->vma);

  return true;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void file_backed_destroy(struct page *page) {
  struct file_page *file_page UNUSED = &page->file;

  struct thread *t = thread_current();
  if (page->frame) {
    pml4_clear_page(t->pml4, page->va);
    page->frame->page = NULL;

    palloc_free_page(page->frame->kva);
    free(page->frame);
  }
}

/* Do the mmap */
void *do_mmap(void *addr, size_t length, int writable, struct file *file,
              off_t offset) {
  ASSERT(addr != NULL);
  ASSERT(pg_ofs(addr) == 0);
  ASSERT(offset % PGSIZE == 0);

  struct file *fp = file_reopen(file);
  if (fp == NULL) {
    return NULL;
  }

  uint8_t *start_addr = addr;
  size_t total_page_count = (length + PGSIZE - 1) / PGSIZE;
  off_t flen = file_length(fp);
  size_t read_bytes = (flen < length) ? flen : length;
  size_t zero_bytes = PGSIZE - (read_bytes % PGSIZE);

  while (read_bytes > 0 || zero_bytes > 0) {
    size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
    size_t page_zero_bytes = PGSIZE - page_read_bytes;

    struct segment_aux *aux = malloc(sizeof *aux);
    if (aux == NULL) {
      file_close(fp);
      return NULL;
    }

    aux->file = fp;
    aux->ofs = offset;
    aux->read_bytes = page_read_bytes;
    aux->zero_bytes = page_zero_bytes;

    if (!vm_alloc_page_with_initializer(VM_FILE, addr, writable,
                                        lazy_load_segment, aux)) {
      free(aux);
      file_close(fp);
      return NULL;
    }

    read_bytes -= page_read_bytes;
    zero_bytes -= page_zero_bytes;
    addr += PGSIZE;
    offset += page_read_bytes;
  }

  return start_addr;
}

static struct mmap_region *mmap_find_region(struct thread *t, void *base) {
  struct list_elem *e;
  for (e = list_begin(&t->mmaps); e != list_end(&t->mmaps); e = list_next(e)) {
    struct mmap_region *rg = list_entry(e, struct mmap_region, elem);
    if (rg->base == base) {
      return rg;
    }
  }
  return NULL;
}

/* Do the munmap */
void do_munmap(void *va) {
  struct thread *t = thread_current();
  struct mmap_region *rg = mmap_find_region(t, va);
  if (rg == NULL) {
    return;
  }

  uint8_t *up = (uint8_t *)rg->base;

  for (size_t i = 0; i < rg->npages; i++, up += PGSIZE) {
    struct page *p = spt_find_page(&t->spt, up);
    if (p == NULL) {
      continue;
    }

    if (VM_TYPE(page_get_type(p)) == VM_FILE && p->frame != NULL) {
      bool dirty = pml4_is_dirty(t->pml4, p->va);
      if (dirty) {
        size_t n = p->file.read_bytes;
        if (n > 0) {
          file_write_at(rg->file, p->frame->kva, n, p->file.ofs);
        }
        pml4_set_dirty(t->pml4, p->va, false);
      }
    }

    spt_remove_page(&t->spt, p);
  }

  file_close(rg->file);
  list_remove(&rg->elem);
  free(rg);
}
