#ifndef VM_FILE_H
#define VM_FILE_H
#include "filesys/file.h"
#include "vm/vm.h"

struct page;
enum vm_type;

struct file_page {
  struct file *file;  // backing file handle
  off_t ofs;          // 파일 오프셋 (PGSIZE 정렬)
  size_t read_bytes;  // 이 페이지에서 파일로부터 읽을 바이트 수
  size_t zero_bytes;  // 나머지 0으로 채울 바이트 수
};

void vm_file_init(void);
bool file_backed_initializer(struct page *page, enum vm_type type, void *kva);
void *do_mmap(void *addr, size_t length, int writable, struct file *file,
              off_t offset);
void do_munmap(void *va);
#endif
