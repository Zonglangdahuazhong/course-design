#include <stdio.h>
#include <stdlib.h>

#include "graph.h"
#include "order.h"
#include "rtree.h"

int main()
{
    // 创建一个叶子节点
    RTreeNode *node = create_node(1);

    if (node == NULL) {
        printf("创建 Node 失败\n");
        return 1;
    }

    // 创建 4 个测试 Order
    Order orders[5];

    for (int i = 0; i < 5; i++) {
        orders[i].id = i + 1;
        orders[i].pointid = i + 1;
    }

    /*
     * 先把前 4 个 Order 转成 Entry，
     * 放进 node。
     *
     * 这里不需要 Graph，
     * 我们直接手动设置 MBR。
     */

    for (int i = 0; i < 4; i++) {

        RTreeEntry entry;

        entry.mbr.min_x = i * 10;
        entry.mbr.max_x = i * 10;
        entry.mbr.min_y = i * 10;
        entry.mbr.max_y = i * 10;

        entry.child = (void *)&orders[i];

        insert_entry(node, entry);
    }

    printf("===== Split 前 =====\n");

    printf("node count = %d\n\n", node->count);

    for (int i = 0; i < node->count; i++) {

        Order *order =
            (Order *)node->entries[i].child;

        printf(
            "Entry %d -> Order %d, pointid = %d\n",
            i,
            order->id,
            order->pointid
        );
    }


    // 创建第 5 个 Entry
    RTreeEntry new_entry;

    new_entry.mbr.min_x = 40;
    new_entry.mbr.max_x = 40;
    new_entry.mbr.min_y = 40;
    new_entry.mbr.max_y = 40;

    new_entry.child = (void *)&orders[4];


    // 两个分裂后的 Node
    RTreeNode *group_a = NULL;
    RTreeNode *group_b = NULL;


    // 执行 Split
    split_node(
        node,
        new_entry,
        &group_a,
        &group_b
    );


    printf("\n===== Split 后 =====\n");

    printf("\n--- Group A ---\n");

    printf("count = %d\n", group_a->count);

    for (int i = 0; i < group_a->count; i++) {

        Order *order =
            (Order *)group_a->entries[i].child;

        printf(
            "Entry %d -> Order %d, pointid = %d\n",
            i,
            order->id,
            order->pointid
        );
    }


    printf("\n--- Group B ---\n");

    printf("count = %d\n", group_b->count);

    for (int i = 0; i < group_b->count; i++) {

        Order *order =
            (Order *)group_b->entries[i].child;

        printf(
            "Entry %d -> Order %d, pointid = %d\n",
            i,
            order->id,
            order->pointid
        );
    }


    // 释放内存
    free(group_a);
    free(group_b);
    free(node);

    return 0;
}