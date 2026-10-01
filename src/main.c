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
RTreeNode *test_node = create_node(1);
RTreeEntry entry;
entry.mbr = order_mbr(&orders[0], graph);
entry.child = &orders[0];
insert_entry(test_node, entry);

printf("count = %d\n", test_node->count);

printf("Entry 0:\n");
printf("min_x = %.8f\n", test_node->entries[0].mbr.min_x);
printf("max_x = %.8f\n", test_node->entries[0].mbr.max_x);
printf("min_y = %.8f\n", test_node->entries[0].mbr.min_y);
printf("max_y = %.8f\n", test_node->entries[0].mbr.max_y);

free(test_node);









}