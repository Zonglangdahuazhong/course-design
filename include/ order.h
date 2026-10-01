#ifndefORDER_H
#define ORDER_H 
#include "graph.h"
typedef struct {
    int id;
    int pointid;
} Order;







Order *generate_orders(const Graph *graph, int count);

void free_orders(Order *orders);

#endif