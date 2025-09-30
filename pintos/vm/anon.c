/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */

#include "devices/disk.h"
#include "threads/vaddr.h"
#include "vm/vm.h"

/* DO NOT MODIFY BELOW LINE */
static struct disk *swap_disk;
static bool anon_swap_in(struct page *page, void *kva);
static bool anon_swap_out(struct page *page);
static void anon_destroy(struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations anon_ops = {
    .swap_in = anon_swap_in,
    .swap_out = anon_swap_out,
    .destroy = anon_destroy,
    .type = VM_ANON,
};

/* Initialize the data for anonymous pages */
void vm_anon_init(void) {
  /* TODO: Set up the swap_disk. */
  swap_disk = NULL;
}

/* Initialize the file mapping */
bool anon_initializer(struct page *page, enum vm_type type, void *kva) {
  /* Set up the handler */
  page->operations = &anon_ops;

  struct anon_page *anon_page = &page->anon;

  return true;
}

/* Swap in the page by read contents from the swap disk. */
static bool anon_swap_in(struct page *page, void *kva) {
  // swap 미구현: 순수 anon page 초기 채움 = zero-fill

  struct anon_page *anon_page = &page->anon;

  memset(kva, 0, PGSIZE);
  return true;
}

/* Swap out the page by writing contents to the swap disk. */
static bool anon_swap_out(struct page *page) {
  /* 현재 단계에서 오면 안 되는 경로 */
  ASSERT(false);  // TODO(swap)

  struct anon_page *anon_page = &page->anon;
  return false;
}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void anon_destroy(struct page *page) {
  /* 현재 단계에서 추가 리소스 없음 (swap 단계에서 메타/슬롯 정리 추가 예정)*/
  struct anon_page *anon_page = &page->anon;
}
