#include"stdio.h"
#include"../include/rtree.h"
#include"stdlib.h"
int main(){
 Graph *graph = mapload("data/map/hit.txt");

    if (graph == NULL) {
        printf("地图加载失败\n");
        return 1;
    }

  
    int count = 4;
    Order *orders = generate(graph, count);

    if (orders == NULL) {
        printf("订单生成失败\n");
       
        return 1;
    }
RTreeNode *root = create_node(1);
insert_order(root, &orders[0], graph);
insert_order(root, &orders[1], graph);
insert_order(root, &orders[2], graph);
insert_order(root, &orders[3], graph);
printf("count = %d\n", root->count);

for (int i = 0; i < root->count; i++) {
    printf("Entry %d:\n", i);
    printf("  min_x = %.8f\n", root->entries[i].mbr.min_x);
    printf("  max_x = %.8f\n", root->entries[i].mbr.max_x);
    printf("  min_y = %.8f\n", root->entries[i].mbr.min_y);
    printf("  max_y = %.8f\n", root->entries[i].mbr.max_y);
}
return 0;


}