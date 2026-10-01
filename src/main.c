#include"stdio.h"
#include"../include/rtree.h"
#include"stdlib.h"
int main(){


RTreeNode *root = create_node(1);

printf("is_leaf = %d\n", root->is_leaf);
printf("count = %d\n", root->count);
free(root);
}