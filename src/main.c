#include <stdio.h>
#include <gtk/gtk.h>
#include"order.h"
#include "../include/graph.h"
#include "../include/gui.h"

int main(int argc, char *argv[])
{
    Graph *graph = mapload("data/map/hit.txt");

    if (graph == NULL) {
        printf("地图加载失败\n");
        return 1;
    }

    printf("地图加载成功\n");
    printf("节点数：%d\n", graph->sum);

    gtk_init(&argc, &argv);

    gui_start(graph);

    return 0;
}