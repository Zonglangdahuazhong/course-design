#include <gtk/gtk.h>
#include <cairo.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "../include/gui.h"
#include "../include/graph.h"
#include "../include/order.h"
#include "../include/rtree.h"
#include "../include/dijkstra.h"
#include "../include/tsp.h"


/* ============================================================
 * MapView
 * ============================================================ */

typedef struct {

    Graph *graph;

    /* 地图原始范围 */
    double min_x;
    double max_x;
    double min_y;
    double max_y;

    /* 当前缩放和平移 */
    double scale;
    double offset_x;
    double offset_y;

    /* 订单 */
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

    Order **query_results;
    int query_result_count;

    /* TSP 路线 */
    Route *route;

    /* 配送中心，Graph 内部使用 0-based */
    int base;

    /* ========================================================
     * 鼠标拖动地图
     * ======================================================== */

    gboolean panning;
    double pan_start_x;
    double pan_start_y;

    double pan_old_offset_x;
    double pan_old_offset_y;

} MapView;


/* ============================================================
 * 坐标变换
 * ============================================================ */

static double to_screen_x(MapView *view, double x)
{
    return view->offset_x +
           (x - view->min_x) * view->scale;
}


static double to_screen_y(MapView *view, double y)
{
    double display_height =
        (view->max_y - view->min_y) * view->scale;

    return view->offset_y +
           display_height -
           (y - view->min_y) * view->scale;
}


/*
 * 屏幕坐标 → 地图坐标
 */

static double to_map_x(MapView *view, double x)
{
    return view->min_x +
           (x - view->offset_x) / view->scale;
}


static double to_map_y(MapView *view, double y)
{
    double display_height =
        (view->max_y - view->min_y) * view->scale;

    return view->min_y +
           (display_height -
            (y - view->offset_y)) /
           view->scale;
}


/* ============================================================
 * 初始化地图范围
 * ============================================================ */

static void calculate_map_bounds(MapView *view)
{
    if (view->graph == NULL ||
        view->graph->sum <= 0) {
        return;
    }

    view->min_x = view->graph->points[0].x;
    view->max_x = view->graph->points[0].x;

    view->min_y = view->graph->points[0].y;
    view->max_y = view->graph->points[0].y;

    for (int i = 1;
         i < view->graph->sum;
         i++) {

        double x = view->graph->points[i].x;
        double y = view->graph->points[i].y;

        if (x < view->min_x)
            view->min_x = x;

        if (x > view->max_x)
            view->max_x = x;

        if (y < view->min_y)
            view->min_y = y;

        if (y > view->max_y)
            view->max_y = y;
    }
}


/* ============================================================
 * 初始化缩放
 * ============================================================ */

static void init_transform(MapView *view,
                           double width,
                           double height)
{
    double map_width =
        view->max_x - view->min_x;

    double map_height =
        view->max_y - view->min_y;

    if (map_width <= 0)
        map_width = 1;

    if (map_height <= 0)
        map_height = 1;

    double margin = 40.0;

    double available_width =
        width - 2.0 * margin;

    double available_height =
        height - 2.0 * margin;

    if (available_width <= 0)
        available_width = width;

    if (available_height <= 0)
        available_height = height;

    double scale_x =
        available_width / map_width;

    double scale_y =
        available_height / map_height;

    view->scale =
        fmin(scale_x, scale_y);

    double display_width =
        map_width * view->scale;

    double display_height =
        map_height * view->scale;

    view->offset_x =
        (width - display_width) / 2.0;

    view->offset_y =
        (height - display_height) / 2.0;
}


/* ============================================================
 * 找最近地图节点
 * ============================================================ */

static int find_nearest_node(MapView *view,
                             double screen_x,
                             double screen_y)
{
    if (view->graph == NULL)
        return -1;

    double best_dist = 1e100;
    int best_node = -1;

    /*
     * 鼠标位置转换成地图坐标
     */
    double map_x =
        to_map_x(view, screen_x);

    double map_y =
        to_map_y(view, screen_y);

    for (int i = 0;
         i < view->graph->sum;
         i++) {

        double dx =
            view->graph->points[i].x -
            map_x;

        double dy =
            view->graph->points[i].y -
            map_y;

        double d =
            dx * dx + dy * dy;

        if (d < best_dist) {
            best_dist = d;
            best_node = i;
        }
    }

    return best_node;
}


/* ============================================================
 * 生成订单
 * ============================================================ */

static void generate_orders(MapView *view,
                            int count)
{
    if (count <= 0)
        return;

    if (view->orders != NULL) {
        free(view->orders);
        view->orders = NULL;
    }

    view->orders =
        generate(view->graph, count);

    if (view->orders == NULL) {
        gtk_label_set_text(
            GTK_LABEL(view->status_label),
            "订单生成失败"
        );
        return;
    }

    view->order_count = count;

    /*
     * R树重新建立
     *
     * 注意：
     * orders 是动态数组，R树中的 Order* 必须
     * 指向新的 orders 数组。
     */

    if (view->rtree == NULL) {

        view->rtree =
            malloc(sizeof(RTree));

        if (view->rtree == NULL) {
            return;
        }

        view->rtree->root = NULL;
    }

    /*
     * 当前版本暂时不调用 rtree_free()
     * 避免之前遇到的内存错误。
     *
     * 这里重新建立 root。
     */

    view->rtree->root = NULL;

    for (int i = 0;
         i < count;
         i++) {

        rtree_insert(
            view->rtree,
            &view->orders[i],
            view->graph
        );
    }

    /*
     * 清空旧查询结果
     */

    if (view->query_results != NULL) {
        free(view->query_results);
        view->query_results = NULL;
    }

    view->query_result_count = 0;
    view->query_area_valid = FALSE;

    /*
     * 清空旧路线
     */

    if (view->route != NULL) {
        free_route(view->route);
        view->route = NULL;
    }

    gtk_label_set_text(
        GTK_LABEL(view->status_label),
        "订单生成完成"
    );

    gtk_widget_queue_draw(view->drawing_area);
}


/* ============================================================
 * R树查询
 * ============================================================ */

static void do_rtree_query(MapView *view)
{
    if (!view->query_area_valid) {

        gtk_label_set_text(
            GTK_LABEL(view->status_label),
            "请先在地图上拖动选择查询区域"
        );

        return;
    }

    if (view->rtree == NULL ||
        view->rtree->root == NULL) {

        gtk_label_set_text(
            GTK_LABEL(view->status_label),
            "R树为空"
        );

        return;
    }

    /*
     * 先释放旧结果
     */

    if (view->query_results != NULL) {
        free(view->query_results);
        view->query_results = NULL;
    }

    if (view->order_count <= 0)
        return;

    view->query_results =
        malloc(view->order_count *
               sizeof(Order *));

    if (view->query_results == NULL) {
        return;
    }

    view->query_result_count =
        rtree_query(
            view->rtree->root,
            view->query_mbr,
            view->query_results,
            view->order_count
        );

    char text[128];

    snprintf(
        text,
        sizeof(text),
        "R树查询完成：找到 %d 个订单",
        view->query_result_count
    );

    gtk_label_set_text(
        GTK_LABEL(view->status_label),
        text
    );

    /*
     * 查询之后旧路线作废
     */

    if (view->route != NULL) {
        free_route(view->route);
        view->route = NULL;
    }

    gtk_widget_queue_draw(view->drawing_area);
}


/* ============================================================
 * 生成配送路线
 *
 * R树结果
 *     ↓
 * generateMatrix()
 *     ↓
 * solve_tsp()
 * ============================================================ */

static void start_delivery(MapView *view)
{
    if (view->query_result_count <= 0) {

        gtk_label_set_text(
            GTK_LABEL(view->status_label),
            "没有查询到订单"
        );

        return;
    }

    /*
     * 清除旧路线
     */

    if (view->route != NULL) {
        free_route(view->route);
        view->route = NULL;
    }

    /*
     * 生成目标点距离矩阵
     *
     * matrix:
     *
     * 0 = base
     * 1 = query_results[0]
     * 2 = query_results[1]
     * ...
     */

    double **matrix =
        generateMatrix(
            view->graph,
            view->query_results,
            view->query_result_count,
            view->base
        );

    if (matrix == NULL) {

        gtk_label_set_text(
            GTK_LABEL(view->status_label),
            "距离矩阵生成失败"
        );

        return;
    }

    /*
     * TSP
     */

    view->route =
        solve_tsp(
            matrix,
            view->query_result_count + 1
        );

    if (view->route == NULL) {

        freeMatrix(
            matrix,
            view->query_result_count + 1
        );

        gtk_label_set_text(
            GTK_LABEL(view->status_label),
            "TSP路线计算失败"
        );

        return;
    }

    char text[256];

    snprintf(
        text,
        sizeof(text),
        "配送路线生成完成，共 %d 个节点，总距离 %.2f",
        view->route->count,
        view->route->distance
    );

    gtk_label_set_text(
        GTK_LABEL(view->status_label),
        text
    );

    freeMatrix(
        matrix,
        view->query_result_count + 1
    );

    gtk_widget_queue_draw(
        view->drawing_area
    );
}


/* ============================================================
 * 画地图道路
 * ============================================================ */

static void draw_map_roads(cairo_t *cr,
                           MapView *view)
{
    cairo_set_line_width(cr, 1.0);

    /*
     * 普通道路
     */

    for (int i = 0;
         i < view->graph->sum;
         i++) {

        double x1 =
            to_screen_x(
                view,
                view->graph->points[i].x
            );

        double y1 =
            to_screen_y(
                view,
                view->graph->points[i].y
            );

        for (Edge *edge =
                 view->graph->adj[i];
             edge != NULL;
             edge = edge->next) {

            int j = edge->to;

            /*
             * 为了避免一条双向道路画两遍，
             * 这里只画 i < j。
             */

            if (j < i)
                continue;

            double x2 =
                to_screen_x(
                    view,
                    view->graph->points[j].x
                );

            double y2 =
                to_screen_y(
                    view,
                    view->graph->points[j].y
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


/* ============================================================
 * 画订单
 * ============================================================ */

static void draw_orders(cairo_t *cr,
                        MapView *view)
{
    if (view->orders == NULL)
        return;

    for (int i = 0;
         i < view->order_count;
         i++) {

        int node =
            view->orders[i].pointid - 1;

        if (node < 0 ||
            node >= view->graph->sum)
            continue;

        double x =
            to_screen_x(
                view,
                view->graph->points[node].x
            );

        double y =
            to_screen_y(
                view,
                view->graph->points[node].y
            );

        /*
         * 判断是不是查询结果
         */

        gboolean selected = FALSE;

        for (int j = 0;
             j < view->query_result_count;
             j++) {

            if (view->query_results[j] ==
                &view->orders[i]) {

                selected = TRUE;
                break;
            }
        }

        /*
         * 查询到的订单画大一点
         */

        if (selected) {
            cairo_arc(
                cr,
                x,
                y,
                5.0,
                0,
                2 * M_PI
            );
        } else {
            cairo_arc(
                cr,
                x,
                y,
                3.0,
                0,
                2 * M_PI
            );
        }

        cairo_fill(cr);

        /*
         * 订单编号
         */

        cairo_move_to(
            cr,
            x + 6,
            y - 6
        );

        cairo_set_font_size(
            cr,
            11
        );

        char text[32];

        snprintf(
            text,
            sizeof(text),
            "%d",
            view->orders[i].id
        );

        cairo_show_text(
            cr,
            text
        );
    }
}


/* ============================================================
 * 画配送中心
 * ============================================================ */

static void draw_base(cairo_t *cr,
                      MapView *view)
{
    int node = view->base;

    if (node < 0 ||
        node >= view->graph->sum)
        return;

    double x =
        to_screen_x(
            view,
            view->graph->points[node].x
        );

    double y =
        to_screen_y(
            view,
            view->graph->points[node].y
        );

    /*
     * 画一个方形表示配送中心
     */

    cairo_rectangle(
        cr,
        x - 7,
        y - 7,
        14,
        14
    );

    cairo_fill(cr);

    cairo_move_to(
        cr,
        x + 10,
        y - 8
    );

    cairo_set_font_size(
        cr,
        13
    );

    cairo_show_text(
        cr,
        "起"
    );
}


/* ============================================================
 * 根据 prev[] 恢复路径
 *
 * start → end
 *
 * prev[end]
 *    ↓
 * prev[...]
 *    ↓
 * start
 *
 * 最后反过来就是：
 *
 * start → ... → end
 * ============================================================ */

static int *reconstruct_path(
    int start,
    int end,
    int *prev,
    int n,
    int *path_length)
{
    int *reverse_path =
        malloc(n * sizeof(int));

    if (reverse_path == NULL)
        return NULL;

    int count = 0;
    int current = end;

    while (current != -1 &&
           count < n) {

        reverse_path[count++] =
            current;

        if (current == start)
            break;

        current = prev[current];
    }

    /*
     * 没有找到 start
     */

    if (count == 0 ||
        reverse_path[count - 1] != start) {

        free(reverse_path);
        return NULL;
    }

    /*
     * 翻转
     */

    int *path =
        malloc(count * sizeof(int));

    if (path == NULL) {

        free(reverse_path);
        return NULL;
    }

    for (int i = 0;
         i < count;
         i++) {

        path[i] =
            reverse_path[count - 1 - i];
    }

    free(reverse_path);

    *path_length = count;

    return path;
}


/* ============================================================
 * 根据 TSP 路线确定实际图节点
 *
 * route index:
 *
 * 0 → base
 * 1 → query_results[0]
 * 2 → query_results[1]
 * ...
 * ============================================================ */

static int route_index_to_graph_node(
    MapView *view,
    int route_index)
{
    if (route_index == 0) {
        return view->base;
    }

    int result_index =
        route_index - 1;

    if (result_index < 0 ||
        result_index >=
            view->query_result_count) {

        return -1;
    }

    return
        view->query_results[result_index]
            ->pointid - 1;
}


/* ============================================================
 * 画真实配送路线
 *
 * TSP：
 *
 * base → order3 → order1 → order2
 *
 * 对每一段：
 *
 * Dijkstra
 *     ↓
 * prev[]
 *     ↓
 * 恢复真实道路
 * ============================================================ */

static void draw_delivery_route(
    cairo_t *cr,
    MapView *view)
{
    if (view->route == NULL)
        return;

    if (view->route->count < 2)
        return;

    int n = view->graph->sum;

    double *dist =
        malloc(n * sizeof(double));

    int *prev =
        malloc(n * sizeof(int));

    if (dist == NULL ||
        prev == NULL) {

        free(dist);
        free(prev);
        return;
    }

    /*
     * 配送路线使用较粗的线
     */

    cairo_set_line_width(
        cr,
        3.5
    );

    /*
     * 逐段处理 TSP 路线
     */

    for (int i = 0;
         i < view->route->count - 1;
         i++) {

        int route_a =
            view->route->route[i];

        int route_b =
            view->route->route[i + 1];

        int start =
            route_index_to_graph_node(
                view,
                route_a
            );

        int end =
            route_index_to_graph_node(
                view,
                route_b
            );

        if (start < 0 ||
            end < 0)
            continue;

        /*
         * 从 start 跑 Dijkstra
         */

        dijkstra(
            view->graph,
            start,
            dist,
            prev
        );

        /*
         * 恢复 start → end 的真实道路
         */

        int path_length = 0;

        int *path =
            reconstruct_path(
                start,
                end,
                prev,
                n,
                &path_length
            );

        if (path == NULL)
            continue;

        /*
         * 把真实道路画出来
         */

        for (int j = 0;
             j < path_length;
             j++) {

            int node =
                path[j];

            double x =
                to_screen_x(
                    view,
                    view->graph->points[node].x
                );

            double y =
                to_screen_y(
                    view,
                    view->graph->points[node].y
                );

            if (j == 0) {

                cairo_move_to(
                    cr,
                    x,
                    y
                );

            } else {

                cairo_line_to(
                    cr,
                    x,
                    y
                );
            }
        }

        cairo_stroke(cr);

        free(path);
    }

    free(dist);
    free(prev);
}


/* ============================================================
 * 显示配送顺序
 *
 * 例如：
 *
 * 起
 *  ↓
 * ①
 *  ↓
 * ②
 *  ↓
 * ③
 * ============================================================ */

static void draw_delivery_order(
    cairo_t *cr,
    MapView *view)
{
    if (view->route == NULL)
        return;

    cairo_set_font_size(
        cr,
        14
    );

    for (int i = 0;
         i < view->route->count;
         i++) {

        int route_index =
            view->route->route[i];

        int node =
            route_index_to_graph_node(
                view,
                route_index
            );

        if (node < 0)
            continue;

        double x =
            to_screen_x(
                view,
                view->graph->points[node].x
            );

        double y =
            to_screen_y(
                view,
                view->graph->points[node].y
            );

        /*
         * 配送中心
         */

        if (i == 0) {

            cairo_move_to(
                cr,
                x + 10,
                y + 18
            );

            cairo_show_text(
                cr,
                "起"
            );

            continue;
        }

        /*
         * 订单配送顺序
         *
         * 使用普通数字：
         *
         * 1
         * 2
         * 3
         *
         * 比 Unicode 圆圈数字更稳定。
         */

        char text[32];

        snprintf(
            text,
            sizeof(text),
            "%d",
            i
        );

        /*
         * 画一个小圆圈
         */

        cairo_arc(
            cr,
            x,
            y,
            9,
            0,
            2 * M_PI
        );

        cairo_stroke(cr);

        /*
         * 数字
         */

        cairo_move_to(
            cr,
            x - 4,
            y + 5
        );

        cairo_show_text(
            cr,
            text
        );
    }
}


/* ============================================================
 * 画查询矩形
 * ============================================================ */

static void draw_query_rectangle(
    cairo_t *cr,
    MapView *view)
{
    if (!view->query_selecting &&
        !view->query_area_valid)
        return;

    double x1 =
        view->drag_start_x;

    double y1 =
        view->drag_start_y;

    double x2 =
        view->drag_end_x;

    double y2 =
        view->drag_end_y;

    double left =
        fmin(x1, x2);

    double top =
        fmin(y1, y2);

    double width =
        fabs(x2 - x1);

    double height =
        fabs(y2 - y1);

    cairo_rectangle(
        cr,
        left,
        top,
        width,
        height
    );

    cairo_stroke(cr);
}


/* ============================================================
 * 总绘图函数
 * ============================================================ */

static gboolean draw_callback(
    GtkWidget *widget,
    cairo_t *cr,
    gpointer data)
{
    MapView *view =
        (MapView *)data;

    /*
     * 背景
     */

    cairo_set_source_rgb(
        cr,
        1.0,
        1.0,
        1.0
    );

    cairo_paint(cr);

    /*
     * 道路
     */

    cairo_set_source_rgb(
        cr,
        0.75,
        0.75,
        0.75
    );

    draw_map_roads(
        cr,
        view
    );

    /*
     * 订单
     */

    cairo_set_source_rgb(
        cr,
        0.1,
        0.1,
        0.8
    );

    draw_orders(
        cr,
        view
    );

    /*
     * 配送中心
     */

    cairo_set_source_rgb(
        cr,
        0.1,
        0.1,
        0.1
    );

    draw_base(
        cr,
        view
    );

    /*
     * 查询框
     */

    cairo_set_source_rgb(
        cr,
        0.2,
        0.5,
        0.9
    );

    draw_query_rectangle(
        cr,
        view
    );

    /*
     * 实际配送路线
     */

    cairo_set_source_rgb(
        cr,
        0.9,
        0.1,
        0.1
    );

    draw_delivery_route(
        cr,
        view
    );

    /*
     * 配送顺序
     */

    cairo_set_source_rgb(
        cr,
        0.9,
        0.1,
        0.1
    );

    draw_delivery_order(
        cr,
        view
    );

    return FALSE;
}


/* ============================================================
 * 鼠标按下
 * ============================================================ */

static gboolean button_press_callback(
    GtkWidget *widget,
    GdkEventButton *event,
    gpointer data)
{
    MapView *view =
        (MapView *)data;

    /*
     * 左键：
     *
     * 如果按住 Ctrl：
     *     选择查询区域
     *
     * 普通左键：
     *     拖动地图
     */

    if (event->button == 1) {

        if (event->state &
            GDK_CONTROL_MASK) {

            /*
             * 开始框选
             */

            view->query_selecting = TRUE;

            view->drag_start_x =
                event->x;

            view->drag_start_y =
                event->y;

            view->drag_end_x =
                event->x;

            view->drag_end_y =
                event->y;

            gtk_widget_queue_draw(
                widget
            );

        } else {

            /*
             * 开始平移地图
             */

            view->panning = TRUE;

            view->pan_start_x =
                event->x;

            view->pan_start_y =
                event->y;

            view->pan_old_offset_x =
                view->offset_x;

            view->pan_old_offset_y =
                view->offset_y;
        }

        return TRUE;
    }

    return FALSE;
}


/* ============================================================
 * 鼠标移动
 * ============================================================ */

static gboolean motion_callback(
    GtkWidget *widget,
    GdkEventMotion *event,
    gpointer data)
{
    MapView *view =
        (MapView *)data;

    /*
     * 框选
     */

    if (view->query_selecting) {

        view->drag_end_x =
            event->x;

        view->drag_end_y =
            event->y;

        gtk_widget_queue_draw(
            widget
        );

        return TRUE;
    }

    /*
     * 平移地图
     */

    if (view->panning) {

        double dx =
            event->x -
            view->pan_start_x;

        double dy =
            event->y -
            view->pan_start_y;

        view->offset_x =
            view->pan_old_offset_x + dx;

        view->offset_y =
            view->pan_old_offset_y + dy;

        gtk_widget_queue_draw(
            widget
        );

        return TRUE;
    }

    return FALSE;
}


/* ============================================================
 * 鼠标释放
 * ============================================================ */

static gboolean button_release_callback(
    GtkWidget *widget,
    GdkEventButton *event,
    gpointer data)
{
    MapView *view =
        (MapView *)data;

    if (event->button != 1)
        return FALSE;

    /*
     * 完成查询框
     */

    if (view->query_selecting) {

        view->query_selecting = FALSE;

        view->drag_end_x =
            event->x;

        view->drag_end_y =
            event->y;

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

        view->query_mbr.min_x =
            fmin(x1, x2);

        view->query_mbr.max_x =
            fmax(x1, x2);

        view->query_mbr.min_y =
            fmin(y1, y2);

        view->query_mbr.max_y =
            fmax(y1, y2);

        view->query_area_valid = TRUE;

        gtk_label_set_text(
            GTK_LABEL(view->status_label),
            "查询区域选择完成，点击“R树查询”"
        );

        gtk_widget_queue_draw(
            widget
        );

        return TRUE;
    }

    /*
     * 结束平移
     */

    if (view->panning) {

        view->panning = FALSE;

        return TRUE;
    }

    return FALSE;
}


/* ============================================================
 * 鼠标滚轮缩放
 *
 * 鼠标所在位置保持不动
 * ============================================================ */

static gboolean scroll_callback(
    GtkWidget *widget,
    GdkEventScroll *event,
    gpointer data)
{
    MapView *view =
        (MapView *)data;

    /*
     * 当前鼠标位置对应的地图坐标
     */

    double mouse_map_x =
        to_map_x(
            view,
            event->x
        );

    double mouse_map_y =
        to_map_y(
            view,
            event->y
        );

    double old_scale =
        view->scale;

    double zoom_factor = 1.15;

    if (event->direction ==
        GDK_SCROLL_UP) {

        view->scale *= zoom_factor;

    } else if (event->direction ==
               GDK_SCROLL_DOWN) {

        view->scale /= zoom_factor;
    }

    /*
     * 限制缩放范围
     */

    if (view->scale < 1.0)
        view->scale = 1.0;

    if (view->scale > old_scale * 1.15)
        view->scale = old_scale * 1.15;

    /*
     * 保证鼠标位置对应的地图点
     * 缩放前后仍然位于鼠标位置
     *
     * screen_x =
     * offset_x +
     * (map_x - min_x) * scale
     */

    view->offset_x =
        event->x -
        (mouse_map_x - view->min_x)
        * view->scale;

    double display_height =
        (view->max_y - view->min_y)
        * view->scale;

    /*
     * screen_y =
     * offset_y +
     * display_height -
     * (map_y - min_y) * scale
     */

    view->offset_y =
        event->y -
        display_height +
        (mouse_map_y - view->min_y)
        * view->scale;

    gtk_widget_queue_draw(
        widget
    );

    return TRUE;
}


/* ============================================================
 * 生成订单按钮
 * ============================================================ */

static void generate_button_callback(
    GtkButton *button,
    gpointer data)
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

    generate_orders(
        view,
        count
    );
}


/* ============================================================
 * R树查询按钮
 * ============================================================ */

static void query_button_callback(
    GtkButton *button,
    gpointer data)
{
    MapView *view =
        (MapView *)data;

    do_rtree_query(view);
}


/* ============================================================
 * 开始配送按钮
 * ============================================================ */

static void delivery_button_callback(
    GtkButton *button,
    gpointer data)
{
    MapView *view =
        (MapView *)data;

    start_delivery(view);
}


/* ============================================================
 * 重置视图
 * ============================================================ */

static void reset_view_callback(
    GtkButton *button,
    gpointer data)
{
    MapView *view =
        (MapView *)data;

    GtkAllocation allocation;

    gtk_widget_get_allocation(
        view->drawing_area,
        &allocation
    );

    init_transform(
        view,
        allocation.width,
        allocation.height
    );

    gtk_widget_queue_draw(
        view->drawing_area
    );

    gtk_label_set_text(
        GTK_LABEL(view->status_label),
        "地图视图已重置"
    );
}


/* ============================================================
 * GUI 初始化
 * ============================================================ */

void gui_start(Graph *graph)
{
    gtk_init(NULL, NULL);

    /*
     * MapView
     */

    MapView *view =
        calloc(1, sizeof(MapView));

    if (view == NULL)
        return;

    view->graph = graph;

    /*
     * 配送中心：
     *
     * Graph 使用 0-based
     *
     * 所以：
     *
     * 地图 ID 1
     *      ↓
     * Graph 下标 0
     */

    view->base = 0;

    calculate_map_bounds(view);

    /*
     * ========================================================
     * Window
     * ========================================================
     */

    GtkWidget *window =
        gtk_window_new(
            GTK_WINDOW_TOPLEVEL
        );

    gtk_window_set_title(
        GTK_WINDOW(window),
        "校园配送路径规划系统"
    );

    gtk_window_set_default_size(
        GTK_WINDOW(window),
        1200,
        800
    );

    g_signal_connect(
        window,
        "destroy",
        G_CALLBACK(gtk_main_quit),
        NULL
    );


    /*
     * ========================================================
     * 主布局
     * ========================================================
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
     * ========================================================
     * 控制栏
     * ========================================================
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
     * 订单数量
     */

    GtkWidget *label =
        gtk_label_new(
            "订单数量："
        );

    gtk_box_pack_start(
        GTK_BOX(control_box),
        label,
        FALSE,
        FALSE,
        5
    );


    view->entry =
        gtk_entry_new();

    gtk_entry_set_text(
        GTK_ENTRY(view->entry),
        "10"
    );

    gtk_widget_set_size_request(
        view->entry,
        80,
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
     * 生成订单
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

    g_signal_connect(
        generate_button,
        "clicked",
        G_CALLBACK(generate_button_callback),
        view
    );


    /*
     * R树查询
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

    g_signal_connect(
        query_button,
        "clicked",
        G_CALLBACK(query_button_callback),
        view
    );


    /*
     * 开始配送
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

    g_signal_connect(
        delivery_button,
        "clicked",
        G_CALLBACK(delivery_button_callback),
        view
    );


    /*
     * 重置地图
     */

    GtkWidget *reset_button =
        gtk_button_new_with_label(
            "重置地图"
        );

    gtk_box_pack_start(
        GTK_BOX(control_box),
        reset_button,
        FALSE,
        FALSE,
        5
    );

    g_signal_connect(
        reset_button,
        "clicked",
        G_CALLBACK(reset_view_callback),
        view
    );


    /*
     * 操作提示
     */

    GtkWidget *help =
        gtk_label_new(
            "Ctrl+左键拖动：框选区域    "
            "左键拖动：移动地图    "
            "滚轮：缩放"
        );

    gtk_box_pack_start(
        GTK_BOX(control_box),
        help,
        FALSE,
        FALSE,
        10
    );


    /*
     * ========================================================
     * 地图 DrawingArea
     * ========================================================
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
        0
    );


    /*
     * 允许鼠标事件
     */

    gtk_widget_add_events(
        view->drawing_area,
        GDK_BUTTON_PRESS_MASK |
        GDK_BUTTON_RELEASE_MASK |
        GDK_POINTER_MOTION_MASK |
        GDK_SCROLL_MASK
    );


    /*
     * 绘图
     */

    g_signal_connect(
        view->drawing_area,
        "draw",
        G_CALLBACK(draw_callback),
        view
    );


    /*
     * 鼠标按下
     */

    g_signal_connect(
        view->drawing_area,
        "button-press-event",
        G_CALLBACK(button_press_callback),
        view
    );


    /*
     * 鼠标移动
     */

    g_signal_connect(
        view->drawing_area,
        "motion-notify-event",
        G_CALLBACK(motion_callback),
        view
    );


    /*
     * 鼠标释放
     */

    g_signal_connect(
        view->drawing_area,
        "button-release-event",
        G_CALLBACK(button_release_callback),
        view
    );


    /*
     * 滚轮
     */

    g_signal_connect(
        view->drawing_area,
        "scroll-event",
        G_CALLBACK(scroll_callback),
        view
    );


    /*
     * ========================================================
     * 状态栏
     * ========================================================
     */

    view->status_label =
        gtk_label_new(
            "Ctrl+左键拖动地图选择R树查询区域"
        );

    gtk_box_pack_start(
        GTK_BOX(main_box),
        view->status_label,
        FALSE,
        FALSE,
        5
    );


    /*
     * ========================================================
     * 显示
     * ========================================================
     */

    gtk_widget_show_all(window);


    /*
     * DrawingArea 现在已经有实际大小，
     * 初始化地图缩放。
     */

    GtkAllocation allocation;

    gtk_widget_get_allocation(
        view->drawing_area,
        &allocation
    );

    init_transform(
        view,
        allocation.width,
        allocation.height
    );


    gtk_main();
}

