#include <stdio.h>
#include <stdlib.h>

#include "graph.h"
#include "order.h"
#include "rtree.h"


/* 打印整棵 R 树 */
void print_tree(RTreeNode *node, int level)
{
    if (node == NULL)
        return;

    /* 缩进 */
    for (int i = 0; i < level; i++)
        printf("    ");

    printf(
        "Node: level=%d, count=%d, is_leaf=%d\n",
        level,
        node->count,
        node->is_leaf
    );

    /* 叶节点 */
    if (node->is_leaf) {

        for (int i = 0; i < node->count; i++) {

            Order *order =
                (Order *)node->entries[i].child;

            for (int j = 0; j < level + 1; j++)
                printf("    ");

            printf(
                "Order: id=%d, pointid=%d\n",
                order->id,
                order->pointid
            );
        }

        return;
    }

    /* 内部节点 */
    for (int i = 0; i < node->count; i++) {

        RTreeNode *child =
            (RTreeNode *)node->entries[i].child;

        print_tree(child, level + 1);
    }
}


int main()
{
    /* =========================
       1. 创建 Graph
       ========================= */

    Graph graph;

    graph.sum = 20;

    graph.points =
        malloc(sizeof(Point) * 20);

    if (graph.points == NULL)
        return 1;


    /* 创建 20 个点 */
    for (int i = 0; i < 20; i++) {

        graph.points[i].id = i + 1;

        graph.points[i].x = i * 10;

        graph.points[i].y = i * 10;
    }


    /* =========================
       2. 创建空 R 树
       ========================= */

    RTree tree;

    tree.root = NULL;


    /* =========================
       3. 创建 20 个订单
       ========================= */

    Order orders[20];

    for (int i = 0; i < 20; i++) {

        orders[i].id = i + 1;

        orders[i].pointid = i + 1;
    }


    /* =========================
       4. 依次插入
       ========================= */

    for (int i = 0; i < 20; i++) {

        printf(
            "\n===== 插入 Order %d =====\n",
            i + 1
        );

        rtree_insert(
            &tree,
            &orders[i],
            &graph
        );

        printf(
            "root count = %d\n",
            tree.root->count
        );

        printf(
            "root is_leaf = %d\n",
            tree.root->is_leaf
        );
    }


    /* =========================
       5. 打印最终整棵树
       ========================= */

    printf("\n\n");
    printf("========== 最终 R 树 ==========\n");

    print_tree(tree.root, 0);


    free(graph.points);

    return 0;
}