#include <stdio.h>
#include <stdlib.h>
#include "graph.h"
#include "order.h"
#include "rtree.h"

int main()
{
    /* ---------- 测试 1：叶节点分裂 ---------- */

    RTreeNode *leaf = create_node(1);

    for (int i = 0; i < 4; i++) {
        RTreeEntry entry;

        entry.mbr.min_x = i;
        entry.mbr.max_x = i;
        entry.mbr.min_y = i;
        entry.mbr.max_y = i;
        entry.child = NULL;

        insert_entry(leaf, entry);
    }

    RTreeEntry new_entry;

    new_entry.mbr.min_x = 10;
    new_entry.mbr.max_x = 10;
    new_entry.mbr.min_y = 10;
    new_entry.mbr.max_y = 10;
    new_entry.child = NULL;

    RTreeNode *group_a = NULL;
    RTreeNode *group_b = NULL;

    split_node(
        leaf,
        new_entry,
        &group_a,
        &group_b
    );

    printf("===== 叶节点分裂 =====\n");
    printf("Group A: count = %d, is_leaf = %d\n",
           group_a->count,
           group_a->is_leaf);

    printf("Group B: count = %d, is_leaf = %d\n",
           group_b->count,
           group_b->is_leaf);


    /* ---------- 测试 2：内部节点分裂 ---------- */

    RTreeNode *internal = create_node(0);

    for (int i = 0; i < 4; i++) {

        RTreeNode *child = create_node(1);

        RTreeEntry entry;

        entry.mbr.min_x = i;
        entry.mbr.max_x = i;
        entry.mbr.min_y = i;
        entry.mbr.max_y = i;

        entry.child = child;

        insert_entry(internal, entry);
    }

    RTreeEntry new_internal_entry;

    new_internal_entry.mbr.min_x = 10;
    new_internal_entry.mbr.max_x = 10;
    new_internal_entry.mbr.min_y = 10;
    new_internal_entry.mbr.max_y = 10;
    new_internal_entry.child = create_node(1);

    group_a = NULL;
    group_b = NULL;

    split_node(
        internal,
        new_internal_entry,
        &group_a,
        &group_b
    );

    printf("\n===== 内部节点分裂 =====\n");
    printf("Group A: count = %d, is_leaf = %d\n",
           group_a->count,
           group_a->is_leaf);

    printf("Group B: count = %d, is_leaf = %d\n",
           group_b->count,
           group_b->is_leaf);


    return 0;
}