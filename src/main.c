#include <stdio.h>
#include <stdlib.h>

#include "graph.h"
#include "order.h"
#include "rtree.h"

int main(void)
{
    Graph *graph = mapload("data//map/hit.txt");

    if (graph == NULL) {
        printf("地图加载失败\n");
        return 1;
    }

    Order orders[5];

    // 这里使用你之前生成的 5 个订单
    orders[0].id = 1;
    orders[0].pointid = 586;

    orders[1].id = 2;
    orders[1].pointid = 655;

    orders[2].id = 3;
    orders[2].pointid = 670;

    orders[3].id = 4;
    orders[3].pointid = 228;

    orders[4].id = 5;
    orders[4].pointid = 1388;

    // 构造 5 个 Entry
    RTreeEntry entries[5];

    for (int i = 0; i < 5; i++) {
        entries[i].mbr =
            order_mbr(&orders[i], graph);

        entries[i].child =
            (void *)&orders[i];
    }

    // 进行 Quadratic Split
    RTreeNode *group_a = NULL;
    RTreeNode *group_b = NULL;

    quadratic_split(
        entries,
        5,
        &group_a,
        &group_b
    );

    // 输出结果
    printf("===== Group A =====\n");

    for (int i = 0; i < group_a->count; i++) {
        Order *order =
            (Order *)group_a->entries[i].child;

        printf(
            "Order %d: pointid = %d\n",
            order->id,
            order->pointid
        );
    }

    printf("\n===== Group B =====\n");

    for (int i = 0; i < group_b->count; i++) {
        Order *order =
            (Order *)group_b->entries[i].child;

        printf(
            "Order %d: pointid = %d\n",
            order->id,
            order->pointid
        );
    }

    printf("\nGroup A count = %d\n", group_a->count);
    printf("Group B count = %d\n", group_b->count);

    free(group_a);
    free(group_b);
    free(graph->points);
    free(graph);

    return 0;
}