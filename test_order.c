#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "order.h"

int main(void)
{
    Graph graph;

    // 1. 初始化并加载地图
     loadMap(&g,"data/map/a.txt");
    // 此处替换成你现有的地图加载函数
    // 例如：loadMap(&graph, "data/hit.txt");

    // 注意：必须确保 graph.sum 已正确初始化

    if(graph.sum <= 0)
    {
        printf("错误：地图中没有顶点！\n");
        return 1;
    }

    // 2. 初始化随机数种子
    srand((unsigned int)time(NULL));

    // 3. 生成5个订单
    int count = 5;

    Order *orders = generate(&graph, count);

    if(orders == NULL)
    {
        printf("订单生成失败！\n");
        return 1;
    }

    // 4. 打印订单
    printf("\n========== 订单生成结果 ==========\n");

    for(int i = 0; i < count; i++)
    {
        printf("订单号：%d  配送点编号：%d\n",
               orders[i].id,
               orders[i].pointid);
    }

    // 5. 验证订单编号和顶点编号
    int passed = 1;

    for(int i = 0; i < count; i++)
    {
        if(orders[i].id != i + 1 ||
           orders[i].pointid < 1 ||
           orders[i].pointid > graph.sum)
        {
            passed = 0;
            printf("订单%d数据异常！\n", i + 1);
        }
    }

    if(passed)
    {
        printf("\n测试通过：5个订单编号及配送点范围均合法！\n");
    }
    else
    {
        printf("\n测试失败！\n");
    }

    // 6. 释放动态内存
    free_orders(orders);

    return passed ? 0 : 1;
}