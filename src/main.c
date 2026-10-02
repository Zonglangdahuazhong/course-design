#include <stdio.h>
#include <stdlib.h>
#include "rtree.h"
int main()
{
    // 创建父节点
    RTreeNode *parent = create_node(0);

    // 创建三个子节点
    RTreeNode *node_a = create_node(1);
    RTreeNode *node_b = create_node(1);
    RTreeNode *node_c = create_node(1);

    // 创建两个分裂后的节点
    RTreeNode *group_a = create_node(1);
    RTreeNode *group_b = create_node(1);

    // 随便构造几个 Entry
    RTreeEntry entry_a;
    entry_a.mbr.min_x = 0;
    entry_a.mbr.min_y = 0;
    entry_a.mbr.max_x = 1;
    entry_a.mbr.max_y = 1;
    entry_a.child = node_a;

    RTreeEntry entry_b;
    entry_b.mbr.min_x = 10;
    entry_b.mbr.min_y = 10;
    entry_b.mbr.max_x = 11;
    entry_b.mbr.max_y = 11;
    entry_b.child = node_b;

    RTreeEntry entry_c;
    entry_c.mbr.min_x = 20;
    entry_c.mbr.min_y = 20;
    entry_c.mbr.max_x = 21;
    entry_c.mbr.max_y = 21;
    entry_c.child = node_c;

    // 父节点：
    // A B C
    insert_entry(parent, entry_a);
    insert_entry(parent, entry_b);
    insert_entry(parent, entry_c);

    printf("===== 替换前 =====\n");

    for (int i = 0; i < parent->count; i++) {
        printf("Entry %d -> child = %p\n",
               i,
               parent->entries[i].child);
    }

    // 假设 B 分裂成 group_a 和 group_b
    replace_entry_with_split(
        parent,
        1,
        group_a,
        group_b
    );

    printf("\n===== 替换后 =====\n");

    for (int i = 0; i < parent->count; i++) {
        printf("Entry %d -> child = %p\n",
               i,
               parent->entries[i].child);
    }

    printf("\nparent count = %d\n", parent->count);

    return 0;
}