#include "map.h"
#include <stdio.h>
#include <time.h>
#include "order.h"

int main()
{

    Graph g;


    loadMap(&g,"a.txt");



    printf("顶点数量:%d\n",
           g.vertexCount);


    printf("边数量:%d\n",
           g.edgeCount);



    printGraph(&g);
     srand((unsigned int)time(NULL));

    // 生成20个订单
    int count = 20;

    Order *orders = generate(&graph, count);

    if(orders == NULL)
    {
        printf("订单生成失败！\n");
        return 1;
    }

    // 打印订单
    for(int i = 0; i < count; i++)
{
    int index = orders[i].pointid;

    printf("订单%d 配送点%d 坐标(%.8f, %.8f)\n",
           orders[i].id,
           index,
           graph.vertices[index].x,
           graph.vertices[index].y);
}

    // 释放内存
    free_orders(orders);



    return 0;
}