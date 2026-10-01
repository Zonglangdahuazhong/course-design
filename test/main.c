#include "map.h"
#include <stdio.h>


int main()
{

    Graph g;


    // 初始化图
    initGraph(&g);



    // 添加道路节点
    addVertex(&g,10,20);   //0号点

    addVertex(&g,30,40);   //1号点

    addVertex(&g,50,60);   //2号点



    // 添加道路
    // 0---5---1
    addEdge(&g,0,1,5);


    // 0---10---2
    addEdge(&g,0,2,10);


    // 1---3---2
    addEdge(&g,1,2,3);



    // 打印图
    printGraph(&g);



    printf("顶点数量:%d\n",g.vertexCount);

    printf("边数量:%d\n",g.edgeCount);



    return 0;
}