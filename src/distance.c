#include <stdio.h>
#include "distance.h"
#include "dijkstra.h"
// 构建订单距离矩阵
int buildDistanceMatrix(const Graph *g,const Order orders[],int count ,double matrix[MAX_ORDERS][MAX_ORDERS]){
    // 检验参数是否合法
    if (g == NULL || orders == NULL || matrix == NULL)
    {
        return 0;
    }
    if(count <=0 || count > MAX_ORDERS){
        return 0;
    } 
    //  检验订单对应点编号是否合法
    for (int i= 0;i<count;i++){
        if(orders[i].pointid < 0 || orders[i].pointid >=g->vertexCount){
            return 0;
        }
    }
    double dist [MAX];
    int path[MAX];
    for (int i=0;i<count;i++){
        int start= orders[i].pointid;
        dijkstra(g,start,dist,path);
        for(int j=0;j<count ;j++)
        {   int end=orders[j].pointid;
            matrix[i][j]=dist[end];
        }
    }
    return 1;
}
// 打印距离矩阵
void printDistanceMatrix(double matrix[MAX_ORDERS][MAX_ORDERS],int count){

    printf("\n========== 订单距离矩阵 ==========\n");

    printf("%10s", "Order");

    // 打印表头
    for(int i = 0; i < count; i++)
    {
        printf("%10d", i + 1);
    }

    printf("\n");


    // 打印矩阵内容
    for(int i = 0; i < count; i++)
    {
        printf("%10d", i + 1);

        for(int j = 0; j < count; j++)
        {
            if(matrix[i][j] >= 1e99)
            {
                printf("%10s", "INF");
            }
            else
            {
                printf("%10.2f", matrix[i][j]);
            }
        }

        printf("\n");
    }
}