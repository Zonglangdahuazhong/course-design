#include "map.h"
#include <stdio.h>

int main()
{

    Graph g;


    loadMap(&g,"a.txt");



    printf("顶点数量:%d\n",
           g.vertexCount);


    printf("边数量:%d\n",
           g.edgeCount);



    printGraph(&g);



    return 0;
}