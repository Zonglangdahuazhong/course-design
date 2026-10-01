#include"stdio.h"
#include"../include/rtree.h"
#include"stdlib.h"
int main(){
 Graph *graph = mapload("data/map/hit.txt");

    if (graph == NULL) {
        printf("地图加载失败\n");
        return 1;
    }

  
    int count = 5;
    Order *orders = generate(graph, count);

    if (orders == NULL) {
        printf("订单生成失败\n");
       
        return 1;
    }
RTreeNode *root = create_node(1);
 RTreeEntry entries[5];

    // 构造 5 个 Entry
    for (int i = 0; i < 5; i++) {
        entries[i].mbr = order_mbr(&orders[i], graph);
        entries[i].child = &orders[i];
    }

    printf("===== 所有 Entry 的 MBR =====\n");

    for (int i = 0; i < 5; i++) {
        printf("Entry %d:\n", i);

        printf("  min_x = %.8f\n", entries[i].mbr.min_x);
        printf("  max_x = %.8f\n", entries[i].mbr.max_x);
        printf("  min_y = %.8f\n", entries[i].mbr.min_y);
        printf("  max_y = %.8f\n", entries[i].mbr.max_y);
    }

    printf("\n===== 两两计算 Waste =====\n");

    for (int i = 0; i < 5; i++) {
        for (int j = i + 1; j < 5; j++) {

            double waste = mbr_waste(
                entries[i].mbr,
                entries[j].mbr
            );

            printf(
                "Entry %d + Entry %d: waste = %.12f\n",
                i, j, waste
            );
        }
    }

    int seed1;
    int seed2;

    pick_seeds(entries, 5, &seed1, &seed2);

    printf("\n===== Seed 选择结果 =====\n");
    printf("Seed 1 = Entry %d\n", seed1);
    printf("Seed 2 = Entry %d\n", seed2);

    return 0;
}