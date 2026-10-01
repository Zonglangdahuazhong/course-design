
#include <stdio.h>
#include "../include/graph.h"
#include "../include/order.h"

int main(void)
{
    // 1. 加载地图
    Graph *graph = mapload("data/map/hit.txt");//文件路径是相对于运行时的工作目录

    if (graph == NULL) {
        printf("地图加载失败\n");
        return 1;
    }

    // 2. 生成 10 个订单
    int count = 10;
    Order *orders = generate(graph, count);

    if (orders == NULL) {
        printf("订单生成失败\n");
       
        return 1;
    }

    // 3. 输出订单信息
    printf("生成 %d 个订单：\n\n", count);

    for (int i = 0; i < count; i++) {
        int pointid = orders[i].pointid;

        printf("Order %d: pointid = %d, x = %.8f, y = %.8f\n",
               orders[i].id,
               pointid,
               graph->points[pointid - 1].x,
               graph->points[pointid - 1].y);
    }

    // 4. 释放内存
    free_orders(orders);
   

    return 0;
}
