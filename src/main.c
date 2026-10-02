#include <stdio.h>
#include "rtree.h"

int main()
{
    RTreeNode *node = create_node(1);

    /* 插入前 4 个 Entry */
    for (int i = 0; i < 4; i++) {

        RTreeEntry entry;

        entry.mbr.min_x = i;
        entry.mbr.min_y = i;
        entry.mbr.max_x = i;
        entry.mbr.max_y = i;
        entry.child = NULL;

        insert_recursive(node, entry);

        printf("插入第 %d 个：node count = %d\n",
               i + 1,
               node->count);
    }

    /* 插入第 5 个，应该触发分裂 */
    RTreeEntry entry;

    entry.mbr.min_x = 10;
    entry.mbr.min_y = 10;
    entry.mbr.max_x = 10;
    entry.mbr.max_y = 10;
    entry.child = NULL;

    RTreeNode *group_b =
        insert_recursive(node, entry);

    printf("\n===== 第 5 个 Entry 插入后 =====\n");

    printf("当前 node count = %d\n",
           node->count);

    printf("当前 node is_leaf = %d\n",
           node->is_leaf);

    printf("返回 group_b count = %d\n",
           group_b->count);

    printf("返回 group_b is_leaf = %d\n",
           group_b->is_leaf);

    return 0;
}