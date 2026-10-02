#include <stdio.h>
#include <stdlib.h>

#include "graph.h"
#include "order.h"
#include "rtree.h"

int main()
{
    /* =========================
       1. 创建一个简单的 Graph
       ========================= */

    Graph graph;

    graph.sum = 5;

    graph.points = malloc(sizeof(Point) * 5);

    if (graph.points == NULL) {
        return 1;
    }

    /* 5 个点，故意放在不同位置 */
    for (int i = 0; i < 5; i++) {
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
       3. 创建 5 个订单
       ========================= */

    Order orders[5];

    for (int i = 0; i < 5; i++) {
        orders[i].id = i + 1;
        orders[i].pointid = i + 1;
    }

    /* =========================
       4. 依次插入
       ========================= */

    for (int i = 0; i < 5; i++) {

        printf("\n===== 插入 Order %d =====\n", i + 1);

        rtree_insert(
            &tree,
            &orders[i],
            &graph
        );

        printf("root count = %d\n",
               tree.root->count);

        printf("root is_leaf = %d\n",
               tree.root->is_leaf);

        /* 如果 root 已经变成内部节点 */
        if (!tree.root->is_leaf) {

            printf("root 有 %d 个子节点\n",
                   tree.root->count);

            for (int j = 0;
                 j < tree.root->count;
                 j++) {

                RTreeNode *child =
                    (RTreeNode *)
                    tree.root->entries[j].child;

                printf(
                    "  child %d: count = %d, is_leaf = %d\n",
                    j,
                    child->count,
                    child->is_leaf
                );
            }
        }
    }

    free(graph.points);

    return 0;
}