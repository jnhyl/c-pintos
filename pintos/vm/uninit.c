/* uninit.c: Implementation of uninitialized page.
 *
 * All of the pages are born as uninit page. When the first page fault occurs,
 * the handler chain calls uninit_initialize (page->operations.swap_in).
 * The uninit_initialize function transmutes the page into the specific page
 * object (anon, file, page_cache), by initializing the page object,and calls
 * initialization callback that passed from vm_alloc_page_with_initializer
 * function.
 * */

#include "vm/uninit.h"

#include "vm/vm.h"

static bool uninit_initialize(struct page *page, void *kva);
static void uninit_destroy(struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations uninit_ops = {
    .swap_in = uninit_initialize,
    .swap_out = NULL,
    .destroy = uninit_destroy,
    .type = VM_UNINIT,
};

/* DO NOT MODIFY this function */
void uninit_new(struct page *page, void *va, vm_initializer *init,
                enum vm_type type, void *aux,
                bool (*initializer)(struct page *, enum vm_type, void *)) {
  ASSERT(page != NULL);

  *page = (struct page){.operations = &uninit_ops,
                        .va = va,
                        .frame = NULL, /* no frame for now */
                        .uninit = (struct uninit_page){
                            .init = init,
                            .type = type,
                            .aux = aux,
                            .page_initializer = initializer,
                        }};
}

/* Initalize the page on first fault */
static bool uninit_initialize(struct page *page, void *kva) {
  struct uninit_page *uninit = &page->uninit;

  /* Fetch first, page_initialize may overwrite the values */
  vm_initializer *init = uninit->init;
  void *aux = uninit->aux;

  bool ok = uninit->page_initializer(page, uninit->type, kva) &&
            (init ? init(page, aux) : true);

  /* 재진입, 이중 free 방지 */
  /* aux는 lazy_load_segment에서 free */
  uninit->init = NULL;
  uninit->aux = NULL;

  return ok;
}

/* Free the resources hold by uninit_page. Although most of pages are transmuted
 * to other page objects, it is possible to have uninit pages when the process
 * exit, which are never referenced during the execution.
 * PAGE will be freed by the caller. */
static void uninit_destroy(struct page *page) {
  struct uninit_page *uninit UNUSED = &page->uninit;
  /* aux를 lazy에서 free 했으면 여기서 할 일 없음 */

  /*
   * uninit page가 한 번도 fault가 나지 않고 프로세스 종료로 uninit_destroy()가
   * 호출되는 경우 lazy_load_segment()가 실행되지 않으니 aux가 해제되지 않은 채
   * 남을 수 있음.

   * TODO: aux != NULL이면 정리하는 로직 추가
   * TODO: 지금은 aux의 설계가 팀원과 논의되지 않아 해제 불가 */
  return;
}
