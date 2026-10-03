#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <gtk/gtk.h>
#include <cairo.h>

#include "../include/gui.h"
#include "../include/graph.h"
#include "../include/order.h"
#include "../include/rtree.h"
#include "../include/dijkstra.h"
#include "../include/tsp.h"


typedef struct {

    Graph *graph;

    double min_x;
    double max_x;
    double min_y;
    double max_y;

    /*
     * 地图显示变换
     *
     * scale：
     *     X、Y统一缩放比例
     *
     * offset_x / offset_y：
     *     地图在窗口中的偏移量
     */
    double scale;
    double offset_x;
    double offset_y;


    /* 所有订单 */
    Order *orders;
    int order_count;


    /* R树 */
    RTree *rtree;


    /* GUI */
    GtkWidget *drawing_area;
    GtkWidget *entry;
    GtkWidget *status_label;


    /* 查询框 */
    gboolean query_selecting;
    gboolean query_area_valid;

    double drag_start_x;
    double drag_start_y;

    double drag_end_x;
    double drag_end_y;

    MBR query_mbr;


    /* R树查询结果 */
    Order **query_results;
    int query_result_count;


    /* TSP最终路线 */
    Route *route;


    /* 配送中心，0-based */
    int base;

} MapView;


/* =========================================================
 * 计算地图到屏幕的变换
 *
 * 保证：
 *
 * 1. 地图完整显示
 * 2. X/Y使用相同缩放比例
 * 3. 地图自动居中
 * 4. 四周留出边距
 * ========================================================= */

static void update_transform(
    MapView *view,
    double width,
    double height
)
{
    double map_width =
        view->max_x - view->min_x;

    double map_height =
        view->max_y - view->min_y;


    /*
     * 防止除0
     */

    if (map_width <= 0) {
        map_width = 1;
    }

    if (map_height <= 0) {
        map_height = 1;
    }


    /*
     * 地图四周留30像素
     */

    double margin = 30.0;


    double available_width =
        width - 2.0 * margin;

    double available_height =
        height - 2.0 * margin;


    if (available_width <= 0) {
        available_width = width;
    }

    if (available_height <= 0) {
        available_height = height;
    }


    /*
     * X和Y分别计算缩放比例
     */

    double scale_x =
        available_width / map_width;

    double scale_y =
        available_height / map_height;


    /*
     * 取较小值
     *
     * 这样整个地图一定能够放进窗口。
     */

    view->scale =
        fmin(scale_x, scale_y);


    /*
     * 地图实际显示尺寸
     */

    double display_width =
        map_width * view->scale;

    double display_height =
        map_height * view->scale;


    /*
     * 居中
     */

    view->offset_x =
        (width - display_width) / 2.0;

    view->offset_y =
        (height - display_height) / 2.0;
}


/* =========================================================
 * 地图坐标 → 屏幕X
 * ========================================================= */

static double to_screen_x(
    MapView *view,
    double x
)
{
    return
        view->offset_x
        +
        (x - view->min_x)
        * view->scale;
}


/* =========================================================
 * 地图坐标 → 屏幕Y
 *
 * 地图坐标：
 *
 *     Y越大越靠上
 *
 * Cairo：
 *
 *     Y越大越靠下
 *
 * 所以需要翻转。
 * ========================================================= */

static double to_screen_y(
    MapView *view,
    double y
)
{
    double display_height =
        (view->max_y - view->min_y)
        * view->scale;


    return
        view->offset_y
        +
        display_height
        -
        (y - view->min_y)
        * view->scale;
}


/* =========================================================
 * 屏幕X → 地图坐标
 * ========================================================= */

static double to_map_x(
    MapView *view,
    double x
)
{
    return
        view->min_x
        +
        (x - view->offset_x)
        / view->scale;
}


/* =========================================================
 * 屏幕Y → 地图坐标
 * ========================================================= */

static double to_map_y(
    MapView *view,
    double y
)
{
    double display_height =
        (view->max_y - view->min_y)
        * view->scale;


    return
        view->min_y
        +
        (
            display_height
            -
            (y - view->offset_y)
        )
        / view->scale;
}


/* =========================================================
 * 找离鼠标最近的地图节点
 * ========================================================= */

static int find_nearest_node(
    MapView *view,
    double sx,
    double sy
)
{
    int nearest = -1;

    double min_dist = 1e100;


    for (int i = 0;
         i < view->graph->sum;
         i++) {

        double x =
            to_screen_x(
                view,
                view->graph->points[i].x
            );


        double y =
            to_screen_y(
                view,
                view->graph->points[i].y
            );


        double dx =
            x - sx;

        double dy =
            y - sy;


        double dist =
            dx * dx + dy * dy;


        if (dist < min_dist) {

            min_dist = dist;
            nearest = i;
        }
    }


    return nearest;
}


/* =========================================================
 * 判断订单是否属于R树查询结果
 * ========================================================= */

static int is_query_result(
    MapView *view,
    Order *order
)
{
    for (int i = 0;
         i < view->query_result_count;
         i++) {

        if (view->query_results[i] == order) {
            return 1;
        }
    }

    return 0;
}


/* =========================================================
 * 重新建立R树
 * ========================================================= */

static void rebuild_rtree(
    MapView *view
)
{
    if (view->rtree == NULL) {

        view->rtree =
            malloc(sizeof(RTree));

        if (view->rtree == NULL) {

            printf("R树内存分配失败\n");
            return;
        }
    }


    /*
     * 暂时不调用 rtree_free()
     *
     * 之前的 rtree_free()
     * 存在内存破坏问题。
     */

    view->rtree->root = NULL;


    for (int i = 0;
         i < view->order_count;
         i++) {

        rtree_insert(
            view->rtree,
            &view->orders[i],
            view->graph
        );
    }


    printf(
        "R树重建完成，共插入 %d 个订单\n",
        view->order_count
    );
}


/* =========================================================
 * 绘制道路
 * ========================================================= */

static void draw_roads(
    cairo_t *cr,
    MapView *view
)
{
    cairo_set_source_rgb(
        cr,
        0.75,
        0.75,
        0.75
    );


    cairo_set_line_width(
        cr,
        1.0
    );


    for (int i = 0;
         i < view->graph->sum;
         i++) {

        Point *p =
            &view->graph->points[i];


        double x1 =
            to_screen_x(
                view,
                p->x
            );


        double y1 =
            to_screen_y(
                view,
                p->y
            );


        for (
            Edge *edge = view->graph->adj[i];
            edge != NULL;
            edge = edge->next
        ) {

            int to =
                edge->to;


            if (to < 0 ||
                to >= view->graph->sum) {
                continue;
            }


            Point *q =
                &view->graph->points[to];


            double x2 =
                to_screen_x(
                    view,
                    q->x
                );


            double y2 =
                to_screen_y(
                    view,
                    q->y
                );


            cairo_move_to(
                cr,
                x1,
                y1
            );


            cairo_line_to(
                cr,
                x2,
                y2
            );


            cairo_stroke(cr);
        }
    }
}


/* =========================================================
 * 绘制订单
 * ========================================================= */

static void draw_orders(
    cairo_t *cr,
    MapView *view
)
{
    for (int i = 0;
         i < view->order_count;
         i++) {

        Order *order =
            &view->orders[i];


        int point_index =
            order->pointid - 1;


        if (point_index < 0 ||
            point_index >= view->graph->sum) {
            continue;
        }


        Point *p =
            &view->graph->points[point_index];


        double x =
            to_screen_x(
                view,
                p->x
            );


        double y =
            to_screen_y(
                view,
                p->y
            );


        /*
         * 查询到的订单：红色
         */

        if (is_query_result(view, order)) {

            cairo_set_source_rgb(
                cr,
                1.0,
                0.1,
                0.1
            );


            cairo_arc(
                cr,
                x,
                y,
                7.0,
                0,
                2 * M_PI
            );


            cairo_fill(cr);
        }


        /*
         * 普通订单：蓝色
         */

        else {

            cairo_set_source_rgb(
                cr,
                0.1,
                0.3,
                0.9
            );


            cairo_arc(
                cr,
                x,
                y,
                4.0,
                0,
                2 * M_PI
            );


            cairo_fill(cr);
        }
    }
}


/* =========================================================
 * 绘制配送中心
 * ========================================================= */

static void draw_base(
    cairo_t *cr,
    MapView *view
)
{
    if (view->base < 0 ||
        view->base >= view->graph->sum) {
        return;
    }


    Point *p =
        &view->graph->points[view->base];


    double x =
        to_screen_x(
            view,
            p->x
        );


    double y =
        to_screen_y(
            view,
            p->y
        );


    cairo_set_source_rgb(
        cr,
        0.1,
        0.7,
        0.1
    );


    cairo_rectangle(
        cr,
        x - 7,
        y - 7,
        14,
        14
    );


    cairo_fill(cr);
}


/* =========================================================
 * 绘制查询区域
 * ========================================================= */

static void draw_query_area(
    cairo_t *cr,
    MapView *view
)
{
    if (!view->query_area_valid &&
        !view->query_selecting) {
        return;
    }


    double left =
        fmin(
            view->drag_start_x,
            view->drag_end_x
        );


    double top =
        fmin(
            view->drag_start_y,
            view->drag_end_y
        );


    double width =
        fabs(
            view->drag_end_x -
            view->drag_start_x
        );


    double height =
        fabs(
            view->drag_end_y -
            view->drag_start_y
        );


    cairo_set_source_rgb(
        cr,
        0.2,
        0.4,
        1.0
    );


    cairo_set_line_width(
        cr,
        2.0
    );


    cairo_rectangle(
        cr,
        left,
        top,
        width,
        height
    );


    cairo_stroke(cr);
}


/* =========================================================
 * 绘制TSP配送路线
 *
 * Route中的route[i]：
 *
 *     不是地图节点ID
 *
 * 而是距离矩阵中的下标：
 *
 *     0 = base
 *     1 = query_results[0]
 *     2 = query_results[1]
 *     ...
 *
 * 注意：
 * 目前这里只画两个目标点之间的直线。
 *
 * Dijkstra实际计算的是道路网络最短路径。
 * ========================================================= */

static void draw_route(
    cairo_t *cr,
    MapView *view
)
{
    if (view->route == NULL) {
        return;
    }


    if (view->route->route == NULL) {
        return;
    }


    if (view->route->count <= 1) {
        return;
    }


    cairo_set_source_rgb(
        cr,
        1.0,
        0.2,
        0.1
    );


    cairo_set_line_width(
        cr,
        3.0
    );


    for (int i = 0;
         i < view->route->count - 1;
         i++) {

        int a =
            view->route->route[i];


        int b =
            view->route->route[i + 1];


        int point_a;
        int point_b;


        /*
         * a
         */

        if (a == 0) {

            point_a =
                view->base;
        }

        else {

            int index =
                a - 1;


            if (index < 0 ||
                index >= view->query_result_count) {
                continue;
            }


            point_a =
                view->query_results[index]
                ->pointid - 1;
        }


        /*
         * b
         */

        if (b == 0) {

            point_b =
                view->base;
        }

        else {

            int index =
                b - 1;


            if (index < 0 ||
                index >= view->query_result_count) {
                continue;
            }


            point_b =
                view->query_results[index]
                ->pointid - 1;
        }


        /*
         * 检查地图节点
         */

        if (point_a < 0 ||
            point_a >= view->graph->sum ||
            point_b < 0 ||
            point_b >= view->graph->sum) {
            continue;
        }


        Point *p1 =
            &view->graph->points[point_a];


        Point *p2 =
            &view->graph->points[point_b];


        double x1 =
            to_screen_x(
                view,
                p1->x
            );


        double y1 =
            to_screen_y(
                view,
                p1->y
            );


        double x2 =
            to_screen_x(
                view,
                p2->x
            );


        double y2 =
            to_screen_y(
                view,
                p2->y
            );


        cairo_move_to(
            cr,
            x1,
            y1
        );


        cairo_line_to(
            cr,
            x2,
            y2
        );


        cairo_stroke(cr);
    }
}


/* =========================================================
 * 总绘图函数
 * ========================================================= */

static void draw_map(
    cairo_t *cr,
    MapView *view
)
{
    /*
     * 背景
     */

    cairo_set_source_rgb(
        cr,
        0.96,
        0.96,
        0.96
    );


    cairo_paint(cr);


    /*
     * 道路
     */

    draw_roads(
        cr,
        view
    );


    /*
     * 订单
     */

    draw_orders(
        cr,
        view
    );


    /*
     * 配送中心
     */

    draw_base(
        cr,
        view
    );


    /*
     * 查询框
     */

    draw_query_area(
        cr,
        view
    );


    /*
     * TSP路线最后画
     */

    draw_route(
        cr,
        view
    );
}


/* =========================================================
 * GTK draw回调
 * ========================================================= */

static gboolean draw_callback(
    GtkWidget *widget,
    cairo_t *cr,
    gpointer data
)
{
    MapView *view =
        (MapView *)data;


    int width =
        gtk_widget_get_allocated_width(
            widget
        );


    int height =
        gtk_widget_get_allocated_height(
            widget
        );


    /*
     * 根据当前窗口大小重新计算
     * 地图缩放比例和偏移量。
     *
     * 所以窗口改变大小时，
     * 地图也会自动重新适应。
     */

    update_transform(
        view,
        width,
        height
    );


    draw_map(
        cr,
        view
    );


    return FALSE;
}


/* =========================================================
 * 生成订单
 * ========================================================= */

static void generate_orders_callback(
    GtkWidget *widget,
    gpointer data
)
{
    MapView *view =
        (MapView *)data;


    const char *text =
        gtk_entry_get_text(
            GTK_ENTRY(view->entry)
        );


    int count =
        atoi(text);


    if (count <= 0) {

        gtk_label_set_text(
            GTK_LABEL(view->status_label),
            "请输入正确的订单数量"
        );

        return;
    }


    /*
     * 删除旧订单
     */

    free(view->orders);

    view->orders = NULL;

    view->order_count = 0;


    /*
     * 生成新订单
     */

    view->orders =
        generate(
            view->graph,
            count
        );


    if (view->orders == NULL) {

        gtk_label_set_text(
            GTK_LABEL(view->status_label),
            "订单生成失败"
        );

        return;
    }


    view->order_count =
        count;


    /*
     * 建立R树
     */

    rebuild_rtree(view);


    /*
     * 清除旧查询结果
     */

    free(view->query_results);

    view->query_results = NULL;

    view->query_result_count = 0;


    /*
     * 清除旧路线
     */

    if (view->route != NULL) {

        free_route(view->route);

        view->route = NULL;
    }


    view->query_area_valid =
        FALSE;


    gtk_label_set_text(
        GTK_LABEL(view->status_label),
        "订单生成完成"
    );


    gtk_widget_queue_draw(
        view->drawing_area
    );
}


/* =========================================================
 * 鼠标按下
 * ========================================================= */

static gboolean map_button_press_callback(
    GtkWidget *widget,
    GdkEventButton *event,
    gpointer data
)
{
    MapView *view =
        (MapView *)data;


    if (event->button != 1) {
        return FALSE;
    }


    view->query_selecting =
        TRUE;


    view->drag_start_x =
        event->x;

    view->drag_start_y =
        event->y;


    view->drag_end_x =
        event->x;

    view->drag_end_y =
        event->y;


    /*
     * 开始新的选择，
     * 清除之前的查询结果
     */

    free(view->query_results);

    view->query_results = NULL;

    view->query_result_count = 0;


    /*
     * 查询区域重新选择后，
     * 原来的路线失效。
     */

    if (view->route != NULL) {

        free_route(view->route);

        view->route = NULL;
    }


    view->query_area_valid =
        FALSE;


    gtk_widget_queue_draw(widget);

    return TRUE;
}


/* =========================================================
 * 鼠标移动
 * ========================================================= */

static gboolean map_motion_callback(
    GtkWidget *widget,
    GdkEventMotion *event,
    gpointer data
)
{
    MapView *view =
        (MapView *)data;


    if (!view->query_selecting) {
        return FALSE;
    }


    view->drag_end_x =
        event->x;

    view->drag_end_y =
        event->y;


    gtk_widget_queue_draw(widget);

    return TRUE;
}


/* =========================================================
 * 鼠标释放
 * ========================================================= */

static gboolean map_button_release_callback(
    GtkWidget *widget,
    GdkEventButton *event,
    gpointer data
)
{
    MapView *view =
        (MapView *)data;


    if (event->button != 1) {
        return FALSE;
    }


    view->query_selecting =
        FALSE;


    view->drag_end_x =
        event->x;

    view->drag_end_y =
        event->y;


    double dx =
        view->drag_end_x -
        view->drag_start_x;


    double dy =
        view->drag_end_y -
        view->drag_start_y;


    double distance =
        sqrt(
            dx * dx +
            dy * dy
        );


    /*
     * =====================================================
     * 点击地图
     * =====================================================
     */

    if (distance < 5.0) {

        int nearest =
            find_nearest_node(
                view,
                event->x,
                event->y
            );


        if (nearest >= 0) {

            /*
             * 扩容订单数组
             */

            Order *new_orders =
                realloc(
                    view->orders,
                    (view->order_count + 1)
                    * sizeof(Order)
                );


            if (new_orders == NULL) {

                printf(
                    "订单数组扩容失败\n"
                );

                return TRUE;
            }


            view->orders =
                new_orders;


            /*
             * 新订单
             */

            Order *order =
                &view->orders[
                    view->order_count
                ];


            order->id =
                view->order_count + 1;


            order->pointid =
                view->graph
                ->points[nearest]
                .id;


            view->order_count++;


            /*
             * orders可能发生realloc，
             * 所以R树中的Order*可能失效。
             *
             * 重新建立R树。
             */

            rebuild_rtree(view);


            /*
             * 清除旧路线
             */

            if (view->route != NULL) {

                free_route(view->route);

                view->route = NULL;
            }


            printf(
                "新增订单：id=%d, pointid=%d\n",
                order->id,
                order->pointid
            );
        }


        gtk_widget_queue_draw(widget);

        return TRUE;
    }


    /*
     * =====================================================
     * 拖拽选择区域
     * =====================================================
     */

    /*
     * 屏幕坐标 → 地图坐标
     */

    double x1 =
        to_map_x(
            view,
            view->drag_start_x
        );


    double y1 =
        to_map_y(
            view,
            view->drag_start_y
        );


    double x2 =
        to_map_x(
            view,
            view->drag_end_x
        );


    double y2 =
        to_map_y(
            view,
            view->drag_end_y
        );


    /*
     * 建立MBR
     */

    view->query_mbr.min_x =
        fmin(x1, x2);


    view->query_mbr.max_x =
        fmax(x1, x2);


    view->query_mbr.min_y =
        fmin(y1, y2);


    view->query_mbr.max_y =
        fmax(y1, y2);


    view->query_area_valid =
        TRUE;


    printf(
        "查询区域：(%f,%f) ~ (%f,%f)\n",
        view->query_mbr.min_x,
        view->query_mbr.min_y,
        view->query_mbr.max_x,
        view->query_mbr.max_y
    );


    gtk_label_set_text(
        GTK_LABEL(view->status_label),
        "查询区域已选择，请点击「R树查询」"
    );


    gtk_widget_queue_draw(widget);

    return TRUE;
}


/* =========================================================
 * R树查询
 * ========================================================= */

static void query_rtree_callback(
    GtkWidget *widget,
    gpointer data
)
{
    MapView *view =
        (MapView *)data;


    if (!view->query_area_valid) {

        gtk_label_set_text(
            GTK_LABEL(view->status_label),
            "请先在地图上拖拽选择查询区域"
        );

        return;
    }


    if (view->rtree == NULL ||
        view->rtree->root == NULL) {

        gtk_label_set_text(
            GTK_LABEL(view->status_label),
            "R树为空，请先生成订单"
        );

        return;
    }


    /*
     * 清除旧结果
     */

    free(view->query_results);

    view->query_results = NULL;

    view->query_result_count = 0;


    if (view->order_count <= 0) {
        return;
    }


    /*
     * 为查询结果分配空间
     */

    Order **results =
        malloc(
            view->order_count
            * sizeof(Order *)
        );


    if (results == NULL) {

        printf(
            "查询结果内存分配失败\n"
        );

        return;
    }


    /*
     * R树查询
     */

    int count =
        rtree_query(
            view->rtree->root,
            view->query_mbr,
            results,
            view->order_count
        );


    view->query_results =
        results;


    view->query_result_count =
        count;


    printf(
        "\n===== R树查询 =====\n"
    );


    printf(
        "找到 %d 个订单\n",
        count
    );


    for (int i = 0;
         i < count;
         i++) {

        printf(
            "Order: id=%d, pointid=%d\n",
            results[i]->id,
            results[i]->pointid
        );
    }


    /*
     * 查询结果变化，
     * 旧路线失效。
     */

    if (view->route != NULL) {

        free_route(view->route);

        view->route = NULL;
    }


    char message[128];


    snprintf(
        message,
        sizeof(message),
        "R树查询完成：找到 %d 个订单",
        count
    );


    gtk_label_set_text(
        GTK_LABEL(view->status_label),
        message
    );


    gtk_widget_queue_draw(
        view->drawing_area
    );
}


/* =========================================================
 * 开始配送
 *
 * 直接调用：
 *
 * generateMatrix()
 * solve_tsp()
 *
 * 不调用make()
 * ========================================================= */

static void start_delivery_callback(
    GtkWidget *widget,
    gpointer data
)
{
    MapView *view =
        (MapView *)data;


    /*
     * 必须先查询订单
     */

    if (view->query_result_count <= 0 ||
        view->query_results == NULL) {

        gtk_label_set_text(
            GTK_LABEL(view->status_label),
            "请先使用R树查询订单"
        );

        return;
    }


    printf(
        "\n===== 开始配送 =====\n"
    );


    printf(
        "配送订单数量：%d\n",
        view->query_result_count
    );


    /*
     * 释放旧路线
     */

    if (view->route != NULL) {

        free_route(view->route);

        view->route = NULL;
    }


    /*
     * =====================================================
     * 生成距离矩阵
     * =====================================================
     */

    double **matrix =
        generateMatrix(
            view->graph,
            view->query_results,
            view->query_result_count,
            view->base
        );


    if (matrix == NULL) {

        printf(
            "距离矩阵生成失败\n"
        );


        gtk_label_set_text(
            GTK_LABEL(view->status_label),
            "距离矩阵生成失败"
        );


        return;
    }


    printf(
        "距离矩阵生成成功\n"
    );


    /*
     * =====================================================
     * TSP
     * =====================================================
     */

    view->route =
        solve_tsp(
            matrix,
            view->query_result_count + 1
        );


    if (view->route == NULL) {

        printf(
            "TSP求解失败\n"
        );


        freeMatrix(
            matrix,
            view->query_result_count + 1
        );


        gtk_label_set_text(
            GTK_LABEL(view->status_label),
            "TSP求解失败"
        );


        return;
    }


    /*
     * 输出TSP结果
     */

    printf(
        "\n===== TSP结果 =====\n"
    );


    print_route(
        view->route
    );


    /*
     * Matrix已经没有用了
     */

    freeMatrix(
        matrix,
        view->query_result_count + 1
    );


    /*
     * route必须保留，
     * 因为GUI还要用它画路线。
     */

    char message[128];


    snprintf(
        message,
        sizeof(message),
        "配送路线计算完成，总距离：%.2f",
        view->route->distance
    );


    gtk_label_set_text(
        GTK_LABEL(view->status_label),
        message
    );


    gtk_widget_queue_draw(
        view->drawing_area
    );
}


/* =========================================================
 * 关闭窗口
 * ========================================================= */

static void destroy_callback(
    GtkWidget *widget,
    gpointer data
)
{
    MapView *view =
        (MapView *)data;


    /*
     * 暂时不要调用rtree_free()
     */

    if (view->rtree != NULL) {

        free(view->rtree);

        view->rtree = NULL;
    }


    /*
     * 查询结果只是指针数组，
     * 不负责释放Order。
     */

    free(view->query_results);

    view->query_results = NULL;


    /*
     * 释放路线
     */

    if (view->route != NULL) {

        free_route(view->route);

        view->route = NULL;
    }


    /*
     * 释放订单数组
     */

    free(view->orders);

    view->orders = NULL;


    /*
     * graph由main负责。
     */

    free(view);


    gtk_main_quit();
}


/* =========================================================
 * GUI入口
 * ========================================================= */

void gui_start(
    Graph *graph
)
{
    gtk_init(NULL, NULL);


    /*
     * 创建MapView
     */

    MapView *view =
        calloc(
            1,
            sizeof(MapView)
        );


    if (view == NULL) {

        printf(
            "MapView内存分配失败\n"
        );

        return;
    }


    view->graph =
        graph;


    /*
     * =====================================================
     * 计算地图范围
     * =====================================================
     */

    view->min_x =
        graph->points[0].x;

    view->max_x =
        graph->points[0].x;

    view->min_y =
        graph->points[0].y;

    view->max_y =
        graph->points[0].y;


    for (int i = 1;
         i < graph->sum;
         i++) {

        double x =
            graph->points[i].x;

        double y =
            graph->points[i].y;


        if (x < view->min_x) {
            view->min_x = x;
        }


        if (x > view->max_x) {
            view->max_x = x;
        }


        if (y < view->min_y) {
            view->min_y = y;
        }


        if (y > view->max_y) {
            view->max_y = y;
        }
    }


    /*
     * 防止除0
     */

    if (view->max_x == view->min_x) {
        view->max_x += 1;
    }


    if (view->max_y == view->min_y) {
        view->max_y += 1;
    }


    /*
     * =====================================================
     * 配送中心
     *
     * Graph内部使用0-based。
     *
     * 所以：
     *
     * 地图节点ID 1
     *        ↓
     * Graph下标 0
     *
     * 如果你的配送中心不是节点1，
     * 只修改这里。
     * =====================================================
     */

    view->base = 0;


    /*
     * =====================================================
     * 创建窗口
     * =====================================================
     */

    GtkWidget *window =
        gtk_window_new(
            GTK_WINDOW_TOPLEVEL
        );


    gtk_window_set_title(
        GTK_WINDOW(window),
        "校园送"
    );


    gtk_window_set_default_size(
        GTK_WINDOW(window),
        1200,
        800
    );


    /*
     * 主布局
     */

    GtkWidget *main_box =
        gtk_box_new(
            GTK_ORIENTATION_VERTICAL,
            5
        );


    gtk_container_add(
        GTK_CONTAINER(window),
        main_box
    );


    /*
     * =====================================================
     * 控制栏
     * =====================================================
     */

    GtkWidget *control_box =
        gtk_box_new(
            GTK_ORIENTATION_HORIZONTAL,
            5
        );


    gtk_box_pack_start(
        GTK_BOX(main_box),
        control_box,
        FALSE,
        FALSE,
        5
    );


    /*
     * 订单数量输入框
     */

    view->entry =
        gtk_entry_new();


    gtk_entry_set_text(
        GTK_ENTRY(view->entry),
        "10"
    );


    gtk_entry_set_placeholder_text(
        GTK_ENTRY(view->entry),
        "订单数量"
    );


    gtk_widget_set_size_request(
        view->entry,
        100,
        -1
    );


    gtk_box_pack_start(
        GTK_BOX(control_box),
        view->entry,
        FALSE,
        FALSE,
        5
    );


    /*
     * 生成订单按钮
     */

    GtkWidget *generate_button =
        gtk_button_new_with_label(
            "生成订单"
        );


    gtk_box_pack_start(
        GTK_BOX(control_box),
        generate_button,
        FALSE,
        FALSE,
        5
    );


    /*
     * R树查询按钮
     */

    GtkWidget *query_button =
        gtk_button_new_with_label(
            "R树查询"
        );


    gtk_box_pack_start(
        GTK_BOX(control_box),
        query_button,
        FALSE,
        FALSE,
        5
    );


    /*
     * 开始配送按钮
     */

    GtkWidget *delivery_button =
        gtk_button_new_with_label(
            "开始配送"
        );


    gtk_box_pack_start(
        GTK_BOX(control_box),
        delivery_button,
        FALSE,
        FALSE,
        5
    );


    /*
     * 状态信息
     */

    view->status_label =
        gtk_label_new(
            "左键点击地图添加订单，拖拽选择查询区域"
        );


    gtk_box_pack_start(
        GTK_BOX(control_box),
        view->status_label,
        FALSE,
        FALSE,
        10
    );


    /*
     * =====================================================
     * 地图区域
     * =====================================================
     */

    view->drawing_area =
        gtk_drawing_area_new();


    gtk_widget_set_hexpand(
        view->drawing_area,
        TRUE
    );


    gtk_widget_set_vexpand(
        view->drawing_area,
        TRUE
    );


    gtk_box_pack_start(
        GTK_BOX(main_box),
        view->drawing_area,
        TRUE,
        TRUE,
        5
    );


    /*
     * =====================================================
     * 鼠标事件
     * =====================================================
     */

    gtk_widget_add_events(
        view->drawing_area,
        GDK_BUTTON_PRESS_MASK |
        GDK_BUTTON_RELEASE_MASK |
        GDK_POINTER_MOTION_MASK
    );


    /*
     * =====================================================
     * 信号
     * =====================================================
     */

    g_signal_connect(
        window,
        "destroy",
        G_CALLBACK(destroy_callback),
        view
    );


    g_signal_connect(
        view->drawing_area,
        "draw",
        G_CALLBACK(draw_callback),
        view
    );


    g_signal_connect(
        view->drawing_area,
        "button-press-event",
        G_CALLBACK(map_button_press_callback),
        view
    );


    g_signal_connect(
        view->drawing_area,
        "motion-notify-event",
        G_CALLBACK(map_motion_callback),
        view
    );


    g_signal_connect(
        view->drawing_area,
        "button-release-event",
        G_CALLBACK(map_button_release_callback),
        view
    );


    g_signal_connect(
        generate_button,
        "clicked",
        G_CALLBACK(generate_orders_callback),
        view
    );


    g_signal_connect(
        query_button,
        "clicked",
        G_CALLBACK(query_rtree_callback),
        view
    );


    g_signal_connect(
        delivery_button,
        "clicked",
        G_CALLBACK(start_delivery_callback),
        view
    );


    /*
     * =====================================================
     * 初始化
     * =====================================================
     */

    view->rtree =
        malloc(sizeof(RTree));


    if (view->rtree != NULL) {
        view->rtree->root = NULL;
    }


    view->orders = NULL;

    view->order_count = 0;

    view->query_results = NULL;

    view->query_result_count = 0;

    view->query_selecting = FALSE;

    view->query_area_valid = FALSE;

    view->route = NULL;


    /*
     * 显示窗口
     */

    gtk_widget_show_all(window);


    gtk_main();
}