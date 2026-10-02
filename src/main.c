#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "graph.h"
#include "order.h"
#include "rtree.h"

int main(void)
{


    srand(time(NULL));
    /* =========================
       1. 加载地图
       ========================= */

    Graph *graph = mapload("data/map/Rtreetest.txt");

    if (graph == NULL) {
        printf("地图加载失败\n");
        return 1;
    }




    /* =========================
       2. 生成订单
       ========================= */

    int order_count = 20;

    Order *orders = generate(
        graph,
        order_count
    );

    if (orders == NULL) {
        printf("订单生成失败\n");
        return 1;
    }

    printf("\n===== 生成订单 =====\n");

    for (int i = 0; i < order_count; i++) {

        printf(
            "Order %d: pointid = %d\n",
            orders[i].id,
            orders[i].pointid
        );
    }


    /* =========================
       3. 创建 R 树
       ========================= */

    RTree tree;

    tree.root = NULL;


    /* =========================
       4. 将订单逐个插入 R 树
       ========================= */

    for (int i = 0; i < order_count; i++) {

        rtree_insert(
            &tree,
            &orders[i],
            graph
        );
    }

    printf("\n===== R树插入完成 =====\n");
    printf("已经插入 %d 个订单\n", order_count);


    /* =========================
       5. 设置查询区域
       ========================= */

    MBR query;

    query.min_x = 15;
    query.min_y = 5;
    query.max_x = 35;
    query.max_y = 25;


    /* =========================
       6. 查询 R 树
       ========================= */

    Order *results[20];

    int count = rtree_query(
        tree.root,
        query,
        results,
        20
    );


    /* =========================
       7. 输出查询结果
       ========================= */

    printf("\n===== R树查询 =====\n");

    printf(
        "查询区域: (%f, %f) ~ (%f, %f)\n",
        query.min_x,
        query.min_y,
        query.max_x,
        query.max_y
    );

    printf(
        "找到 %d 个订单\n",
        count
    );

    for (int i = 0; i < count; i++) {

        printf(
            "Order: id=%d, pointid=%d\n",
            results[i]->id,
            results[i]->pointid
        );
    }


    /* =========================
       8. 释放订单
       ========================= */

    free_orders(orders);

    return 0;
}