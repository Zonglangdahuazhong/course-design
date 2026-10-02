#include <stdio.h>
#include <stdlib.h>
#include"rtree.h"
#include"dijkstra.h"
#include"time.h"
int main(){
srand(time(NULL));


Graph *graph = mapload("data//map/hit.txt");

    if (graph == NULL) {
        printf("地图加载失败\n");
        return 1;
    }

    printf("地图加载成功\n");
    printf("节点数：%d\n", graph->sum);

   int order_count = 10;
    

    
    Order *orders = generate(graph, order_count);
    
    if (orders == NULL) {
        printf("订单内存分配失败\n");
        return 1;
    }
    printf("\n生成 %d 个订单：\n", order_count);
     RTree tree;

    tree.root = NULL;





 for (int i = 0; i < order_count; i++) {

        rtree_insert(
            &tree,
            &orders[i],
            graph
        );
    }
printf("\n===== R树插入完成 =====\n");
    printf("已经插入 %d 个订单\n", order_count);

  MBR query;

    query.min_x = 122;
    query.min_y = 37;
    query.max_x = 123;
    query.max_y = 38;
Order *results[10];

    int count = rtree_query(
        tree.root,
        query,
        results,
        10
    );
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
    int base=310;
double **Matrix=generateMatrix(graph,results,order_count,base);//base:配送中心所在的地图节点在 graph->points[] 中的下标（0-based）
if (Matrix == NULL) {
 
    printf("距离矩阵生成失败\n");
    return 1;
}

int size = order_count + 1;

printf("\n===== 距离矩阵 =====\n");


for (int i = 0; i < size; i++) {

    if (i == 0) {
        printf("%10s", "BASE");
    }
    else {
        printf(
            "%10d",
            results[i - 1]->id
        );
    }
}

printf("\n");
for (int i = 0; i < size; i++) {

    if (i == 0) {
        printf("%10s", "BASE");
    }
    else {
        printf(
            "%10d",
            results[i - 1]->id
        );
    }

    for (int j = 0; j < size; j++) {

        printf(
            "%10.2f",
            Matrix[i][j]
        );
    }

    printf("\n");
}


freeMatrix(
    Matrix,
    size
);

























    
    free_orders(orders);

    return 1;
}







  
