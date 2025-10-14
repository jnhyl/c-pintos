/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */

#include "devices/disk.h"
#include "lib/kernel/bitmap.h"
#include "threads/vaddr.h"
#include "vm/vm.h"

/* DO NOT MODIFY BELOW LINE */
static struct disk *swap_disk;
static bool anon_swap_in(struct page *page, void *kva);
static bool anon_swap_out(struct page *page);
static void anon_destroy(struct page *page);

// 스왑 슬롯 사용 여부 추적용
static struct bitmap *swap_table;

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
  swap_disk = disk_get(1, 1);  //(1:1)이 스왑

  if (swap_disk == NULL) {
    return;
  }

  // swap disk 크기 계산
  disk_sector_t swap_size = disk_size(swap_disk);

  // 슬롯 수 계산
  size_t slot_count = (swap_size / 8);  // 8섹터 = 1슬롯 = 1페이지

  // 비트맵 생성. 각 비트는 하나의 슬롯을 나타냄
  swap_table = bitmap_create(slot_count);
}

/* Initialize the file mapping */
bool anon_initializer(struct page *page, enum vm_type type, void *kva) {
  /* Set up the handler */
  page->operations = &anon_ops;

  struct anon_page *anon_page = &page->anon;

  anon_page->swap_slot = BITMAP_ERROR;  // 0은 안됨, 초기값 설정

  return true;
}

/* Swap in the page by read contents from the swap disk. */
static bool anon_swap_in(struct page *page, void *kva) {
  struct anon_page *anon_page = &page->anon;

  if (anon_page->swap_slot == BITMAP_ERROR) {
    memset(kva, 0, PGSIZE);
    return true;
  }

  for (int i = 0; i < 8; i++) {
    disk_read(swap_disk, (anon_page->swap_slot * 8) + i,
              kva + DISK_SECTOR_SIZE * i);
  }

  bitmap_reset(swap_table, anon_page->swap_slot);
  anon_page->swap_slot = BITMAP_ERROR;
  return true;
}

/* Swap out the page by writing contents to the swap disk. */
static bool anon_swap_out(struct page *page) {
  struct anon_page *anon_page = &page->anon;

  size_t slot_idx = bitmap_scan_and_flip(swap_table, 0, 1, false);

  if (slot_idx == BITMAP_ERROR) {
    PANIC("swap disk is full");
  }

  for (int i = 0; i < 8; i++) {
    disk_write(swap_disk, (slot_idx * 8) + i,
               page->frame->kva + DISK_SECTOR_SIZE * i);
  }

  anon_page->swap_slot = slot_idx;

  return true;
}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void anon_destroy(struct page *page) {
  /* 현재 단계에서 추가 리소스 없음 (swap 단계에서 메타/슬롯 정리 추가 예정)*/
  struct anon_page *anon_page = &page->anon;

  if (anon_page->swap_slot != BITMAP_ERROR) {
    bitmap_reset(swap_table, anon_page->swap_slot);
  }
}
